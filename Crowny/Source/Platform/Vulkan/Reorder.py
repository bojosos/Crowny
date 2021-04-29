header = open("VulkanCommandBuffer.h", "r").read()
cpp = open("VulkanCommandBuffer.cpp", "r").read()

class File:
  def __init__(self, file):
    self.idx = 0
    self.file = file
    self.namespace = ""
    self.classes = []

  def eatChar(self):
    res = self.file[self.idx]
    self.idx += 1
    return res

  def nextChar(self):
    return self.file[self.idx]

  def nextChars(self, text):
    for i in range(len(text)):
      if self.file[self.idx + i] != text[i]:
        return False
    return True

  def consumeWord(self):
    word = ""
    while self.file[self.idx] != " " and self.file[self.idx] != "\n" and self.file[self.idx].isalpha():
      word += self.file[self.idx]
      self.idx += 1
    return word

  def addClass(self, klass):
    self.classes.append(klass)

  def consumeLine(self):
    res = ""
    while self.eatChar() != "\n":
      res += self.file[self.idx - 1]
    return res

  def consumeWhitespace(self):
    while self.file[self.idx] == ' ' or self.file[self.idx] == '\n' or self.file[self.idx] == '\r' or self.file[self.idx] == '\t':
      self.idx += 1

def parseClassDeclaration(header):
  for i in range(10):
   header.consumeWhitespace()
   #print(header.consumeLine())
   
   if header.nextChar() == '{':
     header.eatChar()
   if header.nextChars("private") or header.nextChars("public") or header.nextChars("protected"):
     pass  
   header.consumeWhitespace()

def parseHeaderNamespace(header):
  header.consumeWord()
  header.consumeWhitespace()
  header.namespace = header.consumeWord()
  print("Namespace: " + header.namespace)
  header.consumeWhitespace()
  header.eatChar() # {
  header.consumeWhitespace()
  if header.nextChar() == '#':
    parseCompilerDirective(header)
  while True:
    if header.nextChars("class"):
      header.consumeWhitespace()
      header.consumeWord()
      header.consumeWhitespace()
      className = header.consumeWord()
      header.consumeWhitespace()
      if header.nextChar() == ';':
        print("Forward: class " + className)
        header.eatChar()
        header.consumeWhitespace()
        continue
      parseClassDeclaration(header)
    elif header.nextChars("struct"):
      header.consumeWhitespace()
      header.consumeWord()
      header.consumeWhitespace()
      structName = header.consumeWord()
      header.consumeWhitespace()
      if header.nextChar() == ';':
        print("Forward: struct " + structName)
        header.eatChar()
        header.consumeWhitespace()
        continue
      parseClassDeclaration(header)
    elif header.nextChars("enum"):
      header.consumeWhitespace()
      header.consumeWord()
      header.consumeWhitespace()
      if header.nextChars("class"):
        header.consumeWord()
        print("Enum class")
        header.consumeWhitespace()
        enunName = header.consumeWord()
        header.consumeWhitespace()
      else:
        print("Enum")
        header.consumeWhitespace()
        header.consumeWord()
        enumName = header.consumeWhitespace()
      print(enumName)
    #TODO: Functions, templates, other namespaces
    
  #  klass = Class(className)
    print(className)
    parseClassDeclaration(header)

def parseCompilerDirective(file):
  file.consumeLine()
  file.consumeWhitespace()

def parseHeader(header):
  while header.idx < len(header.file):
    if header.nextChar() == '#':
      parseCompilerDirective(header)

  #  print(header.file[header.idx:])

    while header.nextChars("namespace"):
      parseHeaderNamespace(header)

def parseFile(header, cpp):
  parseHeader(header)

parseFile(File(header), cpp)
