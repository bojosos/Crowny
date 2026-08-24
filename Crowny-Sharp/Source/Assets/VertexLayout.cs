using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    public enum VertexAttribute
    {
        Position = 1,
        Normal = 2,
        Tangent = 3,
        Bitangent = 4,
        Color = 5,
        TexCoord0 = 6,
        TexCoord1 = 7,
        TexCoord2 = 8,
        TexCoord3 = 9,
        TexCoord4 = 10,
        TexCoord5 = 11,
        TexCoord6 = 12,
        TexCoord7 = 13,
        BlendWeights = 14,
        BlendIndices = 15,
        PreviousPosition = 16
    }

    public enum VertexAttributeFormat
    {
        Float32 = 1,
        Float16 = 2,
        UNorm8 = 3,
        SNorm8 = 4,
        UInt8 = 5,
        SInt8 = 6,
        UInt16 = 7,
        SInt16 = 8,
        UInt32 = 9,
        SInt32 = 10
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VertexAttributeDescriptor
    {
        public VertexAttribute attribute;
        public VertexAttributeFormat format;
        public int dimension;
        public int stream;

        public VertexAttributeDescriptor(VertexAttribute attribute, VertexAttributeFormat format = VertexAttributeFormat.Float32, int dimension = 3, int stream = 0)
        {
            this.attribute = attribute;
            this.format = format;
            this.dimension = dimension;
            this.stream = stream;
        }
    }
}
