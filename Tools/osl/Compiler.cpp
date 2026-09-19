// Experimental offline adapter: OSL LLVM group -> value-only Vulkan evaluator.
// OSL/LLVM stay in this tool; the engine consumes validated SPIR-V only.
#include <OSL/oslexec.h>
#include <OSL/oslquery.h>
#include <OSL/rendererservices.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <sstream>
#include <stdexcept>

namespace
{
    struct Input
    {
        const OSL::OSLQuery::Parameter* Parameter;
        std::array<float, 4> Bits{};
    };

    struct TextureInput
    {
        std::string Name, Label;
        uint64_t Hash;
        bool Used = false;
    };

    void LowerTextures(llvm::Module& module, std::vector<TextureInput>& textures)
    {
        llvm::IRBuilder<> b(module.getContext());
        auto* vector = llvm::FixedVectorType::get(b.getFloatTy(), 4);
        std::vector<llvm::CallInst*> calls;
        for (auto& function : module)
            for (auto& block : function)
                for (auto& instruction : block)
                    if (auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction))
                        if (auto* callee = call->getCalledFunction())
                        {
                            const auto name = callee->getName();
                            if (name.starts_with("osl_texture_set_"))
                                throw std::runtime_error("Unsupported OSL texture option: " + name.str());
                            if (name == "osl_texture" || name == "osl_init_texture_options")
                                calls.push_back(call);
                        }
        for (auto* call : calls)
        {
            if (call->getCalledFunction()->getName() == "osl_init_texture_options")
            {
                call->eraseFromParent();
                continue;
            }
            if (call->arg_size() != 18)
                throw std::runtime_error("Unexpected OSL texture ABI");
            const auto* hash = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(1));
            auto texture =
              std::find_if(textures.begin(), textures.end(), [&](const auto& input) { return hash && input.Hash == hash->getZExtValue(); });
            if (texture == textures.end())
                throw std::runtime_error("texture() filename must be a declared string input; dynamic filenames and literals are not supported");
            const auto* channels = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(10));
            if (!channels || (channels->getZExtValue() != 1 && channels->getZExtValue() != 3))
                throw std::runtime_error("texture() must return float or color");
            // Output derivatives require a derivative of the filtered image,
            // not screen-space dFdx inside potentially divergent OSL control flow.
            for (unsigned arg : { 12u, 13u, 15u, 16u, 17u })
                if (!llvm::isa<llvm::ConstantPointerNull>(call->getArgOperand(arg)))
                    throw std::runtime_error("texture() result derivatives and errormessage are not supported");
            texture->Used = true;
            b.SetInsertPoint(call);
            llvm::Value* uv = llvm::Constant::getNullValue(vector);
            uv = b.CreateInsertElement(uv, call->getArgOperand(4), b.getInt32(0));
            uv = b.CreateInsertElement(uv, call->getArgOperand(5), 1u);
            llvm::Value* gradients = llvm::Constant::getNullValue(vector);
            for (unsigned i = 0; i < 4; ++i)
                gradients = b.CreateInsertElement(gradients, call->getArgOperand(6 + i), i);
            const auto sample = module.getOrInsertFunction("crowny_osl_texture_" + std::to_string(texture - textures.begin()),
                                                           llvm::FunctionType::get(vector, { vector, vector }, false));
            auto* rgba = b.CreateCall(sample, { uv, gradients });
            for (unsigned c = 0; c < channels->getZExtValue(); ++c)
                b.CreateStore(b.CreateExtractElement(rgba, c), b.CreateGEP(b.getFloatTy(), call->getArgOperand(11), b.getInt32(c)));
            if (!llvm::isa<llvm::ConstantPointerNull>(call->getArgOperand(14)))
                b.CreateStore(b.CreateExtractElement(rgba, unsigned(channels->getZExtValue())), call->getArgOperand(14));
            call->replaceAllUsesWith(llvm::ConstantInt::get(call->getType(), 1));
            call->eraseFromParent();
        }
    }

    std::string JsonString(const std::string& value)
    {
        std::ostringstream out;
        out << '"';
        for (unsigned char c : value)
            if (c == '"' || c == '\\')
                out << '\\' << c;
            else if (c < 32)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
            else
                out << c;
        return out.str() + '"';
    }

    void WriteInputs(const std::vector<Input>& inputs)
    {
        std::ofstream out("parameters.json");
        out << std::setprecision(9) << "[";
        for (size_t i = 0; i < inputs.size(); ++i)
        {
            const auto& p = *inputs[i].Parameter;
            out << (i ? "," : "") << "{\"Name\":" << JsonString(p.name.string()) << ",\"Type\":" << JsonString(p.type_name()) << ",\"Default\":[";
            for (size_t c = 0; c < p.type.aggregate; ++c)
            {
                if (c)
                    out << ',';
                if (p.type == OIIO::TypeInt)
                    out << p.idefault[c];
                else
                    out << p.fdefault[c];
            }
            out << "]";
            for (const auto& meta : p.metadata)
            {
                if (meta.name == "label" && !meta.sdefault.empty())
                    out << ",\"Label\":" << JsonString(meta.sdefault[0].string());
                if ((meta.name == "min" || meta.name == "max") && (!meta.fdefault.empty() || !meta.idefault.empty()))
                    out << "," << JsonString(meta.name.string()) << ':' << (meta.fdefault.empty() ? float(meta.idefault[0]) : meta.fdefault[0]);
            }
            out << '}';
        }
        out << "]\n";
        if (!out)
            throw std::runtime_error("Cannot write parameter metadata");
    }

    class Errors final : public OIIO::ErrorHandler
    {
    public:
        bool Failed = false;
        void operator()(int code, const std::string& message) override
        {
            if ((code & 0xffff0000) >= EH_ERROR)
                Failed = true;
            std::cerr << message << '\n';
        }
    };

    std::unique_ptr<llvm::Module> ReadModule(const std::filesystem::path& path, llvm::LLVMContext& context)
    {
        llvm::SMDiagnostic error;
        auto module = llvm::parseIRFile(path.string(), error, context);
        if (!module)
        {
            error.print("crowny-osl", llvm::errs());
            throw std::runtime_error("Cannot read LLVM module: " + path.string());
        }
        return module;
    }

    void Optimize(llvm::Module& module)
    {
        llvm::PassBuilder passes;
        llvm::LoopAnalysisManager loops;
        llvm::FunctionAnalysisManager functions;
        llvm::CGSCCAnalysisManager cgscc;
        llvm::ModuleAnalysisManager modules;
        passes.registerModuleAnalyses(modules);
        passes.registerCGSCCAnalyses(cgscc);
        passes.registerFunctionAnalyses(functions);
        passes.registerLoopAnalyses(loops);
        passes.crossRegisterProxies(loops, functions, cgscc, modules);
        passes.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2).run(module, modules);
    }

    void Lower(const std::filesystem::path& runtime, int groupSize, const std::vector<Input>& inputs, std::vector<TextureInput>& textures)
    {
        llvm::LLVMContext context;
        auto module = ReadModule("crowny.bc", context);
        auto shadeops = ReadModule(runtime, context);
        shadeops->setDataLayout(module->getDataLayout());
        shadeops->setTargetTriple(module->getTargetTriple());
        if (llvm::Linker::linkModules(*module, std::move(shadeops)))
            throw std::runtime_error("Cannot link device shadeops");

        llvm::Function* init = nullptr;
        llvm::Function* entry = nullptr;
        for (auto& function : *module)
        {
            if (function.getName().starts_with("osl_init_group_"))
                init = &function;
            if (function.getName().starts_with("osl_layer_group_"))
            {
                if (entry)
                    throw std::runtime_error("This milestone accepts one shader layer");
                entry = &function;
            }
            // Host attributes must never leak into the device compiler.
            function.removeFnAttr("target-cpu");
            function.removeFnAttr("target-features");
            function.removeFnAttr("tune-cpu");
            if (!function.isDeclaration())
            {
                function.setLinkage(llvm::GlobalValue::InternalLinkage);
                function.removeFnAttr(llvm::Attribute::NoInline);
                function.addFnAttr(llvm::Attribute::AlwaysInline);
            }
        }
        if (!init || !entry || groupSize <= 0)
            throw std::runtime_error("Missing OSL group entry or invalid group size");

        llvm::IRBuilder<> b(context);
        auto* vector = llvm::FixedVectorType::get(b.getFloatTy(), 4);
        std::vector<llvm::Type*> arguments(2 + inputs.size(), vector);
        auto* evaluator = llvm::Function::Create(llvm::FunctionType::get(vector, arguments, false), llvm::GlobalValue::ExternalLinkage,
                                                 "crowny_osl_evaluate_material", *module);
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", evaluator));
        auto* globals = b.CreateAlloca(b.getInt8Ty(), b.getInt32(sizeof(OSL::ShaderGlobals)));
        auto* group = b.CreateAlloca(b.getInt8Ty(), b.getInt32(groupSize));
        auto* output = b.CreateAlloca(b.getFloatTy(), b.getInt32(3));
        b.CreateMemSet(globals, b.getInt8(0), sizeof(OSL::ShaderGlobals), llvm::MaybeAlign(1));
        auto set = [&](size_t offset, unsigned argument, unsigned component) {
            auto* address = b.CreateGEP(b.getInt8Ty(), globals, b.getInt64(offset));
            b.CreateStore(b.CreateExtractElement(evaluator->getArg(argument), component), address);
        };
        set(offsetof(OSL::ShaderGlobals, u), 0, 0);
        set(offsetof(OSL::ShaderGlobals, v), 0, 1);
        set(offsetof(OSL::ShaderGlobals, time), 0, 2);
        set(offsetof(OSL::ShaderGlobals, dudx), 1, 0);
        set(offsetof(OSL::ShaderGlobals, dudy), 1, 1);
        set(offsetof(OSL::ShaderGlobals, dvdx), 1, 2);
        set(offsetof(OSL::ShaderGlobals, dvdy), 1, 3);
        auto* null = llvm::ConstantPointerNull::get(b.getPtrTy());
        auto* userdata = b.CreateAlloca(vector, b.getInt32(std::max(size_t(1), inputs.size())));
        for (size_t i = 0; i < inputs.size(); ++i)
            b.CreateStore(evaluator->getArg(2 + i), b.CreateGEP(vector, userdata, b.getInt32(i)));
        b.CreateCall(init, { globals, group, userdata, output, b.getInt32(0), null });
        b.CreateCall(entry, { globals, group, userdata, output, b.getInt32(0), null });
        llvm::Value* result = llvm::Constant::getNullValue(vector);
        for (unsigned i = 0; i != 3; ++i)
            result = b.CreateInsertElement(result, b.CreateLoad(b.getFloatTy(), b.CreateGEP(b.getFloatTy(), output, b.getInt32(i))), i);
        result = b.CreateInsertElement(result, llvm::ConstantFP::get(b.getFloatTy(), 1.0), 3u);
        b.CreateRet(result);

        // Preserve the small compute-test ABI. Live materials call the parameterized entry.
        auto* defaults = llvm::Function::Create(llvm::FunctionType::get(vector, { vector, vector }, false), llvm::GlobalValue::ExternalLinkage,
                                                "crowny_osl_evaluate", *module);
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", defaults));
        std::vector<llvm::Value*> values{ defaults->getArg(0), defaults->getArg(1) };
        for (const auto& input : inputs)
        {
            std::vector<llvm::Constant*> components;
            for (float value : input.Bits)
            {
                uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits));
                components.push_back(llvm::ConstantExpr::getBitCast(b.getInt32(bits), b.getFloatTy()));
            }
            values.push_back(llvm::ConstantVector::get(components));
        }
        b.CreateRet(b.CreateCall(evaluator, values));
        evaluator->addFnAttr(llvm::Attribute::AlwaysInline);

        // libc scalar math has direct SPIR-V equivalents through LLVM intrinsics.
        for (const auto& mapping : { std::pair{ "sinf", llvm::Intrinsic::sin },
                                     { "cosf", llvm::Intrinsic::cos },
                                     { "sqrtf", llvm::Intrinsic::sqrt },
                                     { "floorf", llvm::Intrinsic::floor },
                                     { "fabsf", llvm::Intrinsic::fabs } })
        {
            if (auto* function = module->getFunction(mapping.first))
            {
                auto* intrinsic = llvm::Intrinsic::getOrInsertDeclaration(module.get(), mapping.second, { b.getFloatTy() });
                function->replaceAllUsesWith(intrinsic);
            }
        }
        llvm::StripDebugInfo(*module);
        // Header-only C++ runtime code can carry unrelated locale constructors.
        // Device code cannot run host constructors; only the evaluator is a root.
        for (const char* name : { "llvm.global_ctors", "llvm.global_dtors", "llvm.used", "llvm.compiler.used" })
            if (auto* global = module->getNamedGlobal(name))
                global->eraseFromParent();
        for (auto& global : module->globals())
            if (!global.isDeclaration())
            {
                global.setLinkage(llvm::GlobalValue::InternalLinkage);
                global.setComdat(nullptr);
            }
        Optimize(*module);
        LowerTextures(*module, textures);
        // OSL addresses preplaced userdata as inttoptr(ptrtoint(base) + offset).
        // Restore byte GEPs so SROA can eliminate the local parameter storage.
        for (auto& function : *module)
            for (auto& block : function)
                for (auto iterator = block.begin(); iterator != block.end();)
                {
                    auto* cast = llvm::dyn_cast<llvm::IntToPtrInst>(&*iterator++);
                    if (!cast)
                        continue;
                    auto* add = llvm::dyn_cast<llvm::BinaryOperator>(cast->getOperand(0));
                    if (!add || add->getOpcode() != llvm::Instruction::Add)
                        continue;
                    auto* base = llvm::dyn_cast<llvm::PtrToIntInst>(add->getOperand(0));
                    auto* offset = llvm::dyn_cast<llvm::ConstantInt>(add->getOperand(1));
                    if (!base || !offset || !offset->getType()->isIntegerTy(64))
                        continue;
                    llvm::Value* root = base->getPointerOperand();
                    while (auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(root))
                    {
                        if (!gep->hasAllConstantIndices())
                            break;
                        root = gep->getPointerOperand();
                    }
                    if (!llvm::isa<llvm::AllocaInst>(root))
                        continue;
                    b.SetInsertPoint(cast);
                    cast->replaceAllUsesWith(b.CreateGEP(b.getInt8Ty(), base->getPointerOperand(), offset));
                    cast->eraseFromParent();
                }
        Optimize(*module);
        std::error_code diagnosticError;
        llvm::raw_fd_ostream diagnostic("lowered-host.ll", diagnosticError);
        if (!diagnosticError)
            module->print(diagnostic, nullptr);
        for (auto& global : module->globals())
            if (!global.use_empty())
                throw std::runtime_error("Unsupported device global: " + global.getName().str());
        // Reject unresolved renderer calls and pointer operations instead of producing
        // a shader with host addresses, silent default services, or invalid Vulkan IR.
        for (auto& function : *module)
        {
            if (function.isDeclaration() && !function.use_empty() && !function.isIntrinsic() &&
                !function.getName().starts_with("crowny_osl_texture_"))
                throw std::runtime_error("Unsupported device shadeop: " + function.getName().str());
            for (auto& block : function)
                for (auto& instruction : block)
                {
                    if (instruction.getType()->isPointerTy() || llvm::isa<llvm::PtrToIntInst>(instruction))
                        throw std::runtime_error("Host memory survived scalarization in " + function.getName().str());
                    // LLVM 20 emits invalid Vulkan OpSpecConstantOp Bitcast for
                    // poison vector lanes. Give unused lanes a concrete value.
                    for (auto& operand : instruction.operands())
                        if (auto* constant = llvm::dyn_cast<llvm::Constant>(operand.get()))
                        {
                            if (auto* type = llvm::dyn_cast<llvm::FixedVectorType>(constant->getType()))
                            {
                                llvm::SmallVector<llvm::Constant*> elements;
                                for (unsigned i = 0; i < type->getNumElements(); ++i)
                                {
                                    auto* element = constant->getAggregateElement(i);
                                    if (!element)
                                        throw std::runtime_error("Unsupported vector constant expression");
                                    elements.push_back(llvm::isa<llvm::UndefValue>(element) ? llvm::Constant::getNullValue(type->getElementType())
                                                                                            : element);
                                }
                                operand.set(llvm::ConstantVector::get(elements));
                            }
                            else if (llvm::isa<llvm::UndefValue>(constant))
                                operand.set(llvm::Constant::getNullValue(constant->getType()));
                        }
                }
        }
        // Module metadata may contain CPU-only globals and Windows linker directives.
        while (!module->named_metadata_empty())
            module->eraseNamedMetadata(&*module->named_metadata_begin());
        module->setTargetTriple("spirv-unknown-unknown");
        module->setDataLayout("");
        if (llvm::verifyModule(*module, &llvm::errs()))
            throw std::runtime_error("Invalid lowered LLVM module");
        std::error_code error;
        llvm::raw_fd_ostream file("evaluator.ll", error);
        if (error)
            throw std::runtime_error(error.message());
        module->print(file, nullptr);
    }

    void Compile(const std::filesystem::path& shader, const std::string& outputName, const std::filesystem::path& runtime)
    {
        std::filesystem::remove("crowny.bc");
        OSL::OSLQuery query;
        if (!query.open(shader.string()))
            throw std::runtime_error("Cannot inspect shader: " + query.geterror());
        const auto* parameter = query.getparam(outputName);
        if (!parameter || !parameter->isoutput || parameter->type != OIIO::TypeColor)
            throw std::runtime_error("Select a color output parameter for this milestone");
        Errors errors;
        OSL::RendererServices renderer;
        OSL::ShadingSystem system(&renderer, nullptr, &errors);
        system.attribute("llvm_output_bitcode", 1);
        system.attribute("optimize", 2);
        system.attribute("lockgeom", 1);
        system.attribute("opt_texture_handle", 0);
        auto group = system.ShaderGroupBegin("crowny");
        std::vector<Input> inputs;
        std::vector<TextureInput> textures;
        std::vector<OSL::SymLocationDesc> locations;
        for (const auto& p : query.parameters())
        {
            if (p.isoutput)
                continue;
            if (p.type == OIIO::TypeString && p.validdefault && !p.sdefault.empty())
            {
                if (!p.sdefault[0].empty())
                    throw std::runtime_error("OSL texture inputs require an empty default; assign a texture asset in the material: " +
                                             p.name.string());
                if (textures.size() == 8)
                    throw std::runtime_error("At most 8 OSL texture inputs are supported");
                const OIIO::ustring token("__crowny_texture_" + p.name.string());
                std::string label = p.name.string();
                for (const auto& meta : p.metadata)
                    if (meta.name == "label" && !meta.sdefault.empty())
                        label = meta.sdefault[0].string();
                textures.push_back({ p.name.string(), label, token.hash() });
                if (!system.Parameter(p.name, p.type, &token))
                    throw std::runtime_error("Cannot bind OSL texture input: " + p.name.string());
                continue;
            }
            if (p.isstruct || p.isclosure || p.type.is_array() || !p.validdefault ||
                !(p.type == OIIO::TypeFloat || p.type == OIIO::TypeInt || p.type == OIIO::TypeColor || p.type == OIIO::TypePoint ||
                  p.type == OIIO::TypeVector || p.type == OIIO::TypeNormal) ||
                !p.spacename.empty())
                throw std::runtime_error("Unsupported editable OSL input: " + p.name.string() + " (" + p.type_name() + ")");
            if (inputs.size() == 32)
                throw std::runtime_error("At most 32 editable OSL inputs are supported");
            Input input{ &p };
            const void* data = p.type == OIIO::TypeInt ? static_cast<const void*>(p.idefault.data()) : p.fdefault.data();
            std::memcpy(input.Bits.data(), data, p.type.size());
            if (!system.Parameter(p.name, p.type, data, OSL::ParamHints::interpolated))
                throw std::runtime_error("Cannot bind OSL input: " + p.name.string());
            locations.emplace_back(p.name, p.type, false, OSL::SymArena::UserData, inputs.size() * 16, 0);
            inputs.push_back(input);
        }
        if (!system.Shader("surface", shader.string(), "texture") || !system.ShaderGroupEnd())
            throw std::runtime_error("Cannot create OSL shader group");
        const OSL::SymLocationDesc location(outputName, OIIO::TypeColor, false, OSL::SymArena::Outputs, 0, 12);
        system.add_symlocs(group.get(), { &location, 1 });
        system.add_symlocs(group.get(), locations);
        auto* thread = system.create_thread_info();
        auto* ctx = system.get_context(thread);
        system.optimize_group(group.get(), ctx);
        int globals = 0;
        system.getattribute(group.get(), "globals_read", globals);
        const int supported = int(OSL::SGBits::u) | int(OSL::SGBits::v) | int(OSL::SGBits::time);
        if (globals & ~supported)
            throw std::runtime_error("Only u, v and time globals are supported by sample ABI v1");
        for (const char* name :
             { "num_closures_needed", "unknown_closures_needed", "unknown_textures_needed", "num_attributes_needed", "unknown_attributes_needed" })
        {
            int count = 0;
            if (system.getattribute(group.get(), name, count) && count)
                throw std::runtime_error(std::string("Unsupported renderer service: ") + name);
        }
        OSL::ShaderGlobals sg{};
        float color[3]{};
        std::vector<std::array<float, 4>> inputValues;
        for (const auto& input : inputs)
            inputValues.push_back(input.Bits);
        // run=false asks OSL to finish LLVM code generation and setup only.
        // No shading point is evaluated here; CPU reference evaluation lives
        // exclusively in the separate Reference.cpp test executable.
        if (!system.execute(*ctx, *group, 0, 0, sg, inputValues.data(), color, false) || errors.Failed)
            throw std::runtime_error("OSL group compilation failed");
        int groupSize = 0;
        system.getattribute(group.get(), "llvm_groupdata_size", groupSize);
        if (!std::filesystem::exists("crowny.bc"))
            throw std::runtime_error("OSL must be built with Tools/osl/export-bitcode.patch");
        Lower(runtime, groupSize, inputs, textures);
        WriteInputs(inputs);
        std::ofstream textureMetadata("textures.json");
        textureMetadata << '[';
        for (size_t i = 0; i < textures.size(); ++i)
        {
            const auto& texture = textures[i];
            if (!texture.Used)
                throw std::runtime_error("Unsupported editable OSL input: " + texture.Name + " (string input is not used by texture())");
            textureMetadata << (i ? "," : "") << "{\"Name\":" << JsonString(texture.Name) << ",\"Label\":" << JsonString(texture.Label) << '}';
        }
        textureMetadata << "]\n";
        if (!textureMetadata)
            throw std::runtime_error("Cannot write texture input metadata");
        system.release_context(ctx);
        system.destroy_thread_info(thread);
        std::cout << "Lowered OSL material with " << inputs.size() << " numeric inputs and " << textures.size() << " texture inputs\n";
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: crowny-osl shader.oso color-output shadeops.bc\nRun in a fresh output directory.\n";
        return 2;
    }
    try
    {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        Compile(std::filesystem::absolute(argv[1]), argv[2], std::filesystem::absolute(argv[3]));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "OSL Vulkan compilation failed: " << error.what() << '\n';
        return 1;
    }
}
