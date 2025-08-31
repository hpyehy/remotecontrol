#pragma once
class CTool
{
public:
    static void Dump(BYTE* pData, size_t nSize)
    {//以十六进制格式打印数据
        std::string strOut;
        for (size_t i = 0; i < nSize; i++)
        {
            char buf[8] = "";
            if (i > 0 && (i % 16 == 0))strOut += "\n";// 每行显示16个子节
            snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
            strOut += buf;
        }
        strOut += "\n";
        OutputDebugStringA(strOut.c_str());
    }
};

