//
// Created by Raul Romero on 2024-01-31.
//

#include "Reader.h"
#include <fstream>

namespace photon
{

#if defined(__EMSCRIPTEN__)
#define RESOURCE_PATH "res/"
#else
#define RESOURCE_PATH "../res/"
#endif

Reader::Reader(const EReaderType type, const EReaderMode mode, const EReaderFormat format) :
    m_Type(type),
    m_Mode(mode),
    m_Format(format)
{}

Reader& Reader::TextFileReader()
{
    static Reader reader(EReaderType::File, EReaderMode::Read, EReaderFormat::Text);
    return reader;
}

Reader& Reader::CreateBinaryFileReader()
{
    static Reader reader(EReaderType::File, EReaderMode::Read, EReaderFormat::Binary);
    return reader;
}

Reader& Reader::CreateTextMemoryReader()
{
    static Reader reader(EReaderType::Memory, EReaderMode::Read, EReaderFormat::Text);
    return reader;
}

Reader& Reader::CreateBinaryMemoryReader()
{
    static Reader reader(EReaderType::Memory, EReaderMode::Read, EReaderFormat::Binary);
    return reader;
}

bool Reader::Open(const std::string& path)
{
    if (m_Type != EReaderType::File)
    {
        printf("Reader::Open: Reader type is not File\n");
        return false;
    }

    int flags = 0;
    if (m_Mode == EReaderMode::Read)
    {
        flags |= std::ios::in;
    }
    else if (m_Mode == EReaderMode::Write)
    {
        flags |= std::ios::out;
    }
    else if (m_Mode == EReaderMode::All)
    {
        flags |= std::ios::in | std::ios::out;
    }

    if (m_Format == EReaderFormat::Binary)
    {
        flags |= std::ios::binary;
    }

    std::string fullPath = std::string(RESOURCE_PATH) + path;
    m_File.open(fullPath, flags);
    if (!m_File.is_open())
    {
        printf("Reader::Open: Failed to open file %s\n", path.c_str());
        return false;
    }
    return true;
}

void Reader::Close()
{
    if (m_Type != EReaderType::File)
    {
        printf("Reader::Close: Reader type is not File\n");
        return;
    }

    if (!m_File.is_open())
    {
        return;
    }

    m_File.close();
}

std::string Reader::Read()
{
    if (m_Type != EReaderType::File)
    {
        printf("Reader::Read: Reader type is not File\n");
        return "";
    }

    std::string line;
    std::string content;
    while (std::getline(m_File, line))
    {
        content += line + "\n";
    }

    return content;
}

std::string Reader::Read(const std::string &path)
{
    Open(path);
    return Read();
}

const char* Reader::ReadCStr()
{
    Reader& reader = Reader::TextFileReader();
    reader.SetMemory(reader.Read().c_str());
    return reader.GetMemory();
}

const char *Reader::ReadTextFile(const std::string &path)
{
    Reader::TextFileReader().Open(path);
    return ReadCStr();
}

Reader::~Reader()
{
    ClearMemory();
    Close();
}

bool Reader::IsMemoryValid() const
{
    return m_Memory != nullptr;
}

void Reader::ClearMemory()
{
    if(IsMemoryValid())
    {
        delete[] m_Memory;
        m_Memory = nullptr;
    }
}

void Reader::SetMemory(const char *memory)
{
    ClearMemory();
    m_Memory = new char[strlen(memory) + 1];
    strcpy(m_Memory, memory);
}

const char *Reader::GetMemory()
{
    return m_Memory;
}

}