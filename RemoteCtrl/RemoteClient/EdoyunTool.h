#pragma once

#include<string>
#include <Windows.h>
#include <atlimage.h>


class CEdoyunTool
{
public:
    static void Dump(BYTE* pData, size_t nSize)
    {
        std::string strOut;
        for (size_t i = 0; i < nSize; i++)
        {
            char buf[8] = "";
            if (i > 0 && (i % 16 == 0)) strOut += "\n";
            snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
            strOut += buf;
        }
        strOut += "\n";
        OutputDebugStringA(strOut.c_str());
    }

    static int Bytes2Image(CImage& image, const std::string& strBuffer) {
		
		BYTE* pData = (BYTE*)strBuffer.c_str();  //获取返回的数据包数据

		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);  //分配可移动的全局内存块
		if (hMem == NULL) {
			TRACE("内存不足了！\r\n");
			Sleep(1);
			return -1;
		}

		IStream* pStream = NULL;
		HRESULT hRet = CreateStreamOnHGlobal(hMem, TRUE, &pStream);  //创建基于全局内存的IStream流对象
		if (hRet == S_OK) {
			ULONG length = 0;
			pStream->Write(pData, strBuffer.size(), &length); //将Socket接收的二进制图片数据写入流中
			LARGE_INTEGER bg = { 0 };        // 设置流的读取位置到起始处（确保图片加载时从开头读取）
			pStream->Seek(bg, STREAM_SEEK_SET, NULL);

			if ((HBITMAP)image != NULL) { image.Destroy(); }
			image.Load(pStream);               // 将流中的图片数据加载到m_image对象

		}
		return hRet;

    }
};



