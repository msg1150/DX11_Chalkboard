// [칠판 프로젝트에서의 역할]
// 실행 파일 옆 HLSL을 런타임 컴파일하고 진단 메시지를 보존합니다.
// Vertex 레이아웃은 Position/Normal/UV/Color의 순서와 바이트 오프셋을 맞춰야 합니다.

// ============================================================================
// Shader.cpp
// ----------------------------------------------------------------------------
// HLSL 런타임 컴파일과 Direct3D Shader Object 생성을 담당한다.
//
// HLSL Compile이 실패하면 errorBlob의 내용을 문자열로 변환해
// Application 진단 팝업 및 Visual Studio Output에서 확인할 수 있게 한다.
// ============================================================================

#include "Graphics/Shader.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <Windows.h>

#include <array>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
    // HRESULT를 사람이 비교하기 쉬운 16진수 문자열로 만든다.
    std::wstring HResultToString(HRESULT hr)
    {
        std::wstringstream stream;

        stream
            << L"0x"
            << std::uppercase
            << std::hex
            << static_cast<unsigned long>(hr);

        return stream.str();
    }

    // D3DCompiler의 errorBlob은 char* 문자열이다.
    // 먼저 UTF-8로 변환하고, 실패하면 시스템 ANSI Code Page로 재시도한다.
    std::wstring BlobTextToWide(ID3DBlob* blob)
    {
        if (!blob ||
            blob->GetBufferSize() == 0)
        {
            return L"";
        }

        const char* text =
            static_cast<const char*>(
                blob->GetBufferPointer());

        const int byteCount =
            static_cast<int>(
                blob->GetBufferSize());

        auto convert =
            [text, byteCount](UINT codePage) -> std::wstring
        {
            const int required =
                MultiByteToWideChar(
                    codePage,
                    0,
                    text,
                    byteCount,
                    nullptr,
                    0);

            if (required <= 0)
            {
                return L"";
            }

            std::wstring result(
                static_cast<size_t>(required),
                L'\0');

            MultiByteToWideChar(
                codePage,
                0,
                text,
                byteCount,
                result.data(),
                required);

            // D3DCompiler 메시지 끝에 포함될 수 있는 Null 문자를 제거한다.
            while (!result.empty() &&
                   result.back() == L'\0')
            {
                result.pop_back();
            }

            return result;
        };

        std::wstring result =
            convert(CP_UTF8);

        if (result.empty())
        {
            result =
                convert(CP_ACP);
        }

        return result;
    }

    // 하나의 HLSL 파일을 지정한 Entry Point / Shader Model로 컴파일한다.
    bool CompileShader(
        const std::filesystem::path& path,
        const char* entryPoint,
        const char* target,
        Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob,
        std::wstring& outError)
    {
        outError.clear();

        // Post-Build 복사가 실패했는지 바로 구분할 수 있게
        // Compile 전에 파일 존재 여부를 먼저 검사한다.
        if (!std::filesystem::exists(path))
        {
            outError =
                L"Shader 파일을 찾을 수 없습니다.\n"
                L"Path: " +
                path.wstring();

            return false;
        }

        UINT compileFlags =
            D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
        compileFlags |=
            D3DCOMPILE_DEBUG |
            D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

        const HRESULT hr =
            D3DCompileFromFile(
                path.c_str(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entryPoint,
                target,
                compileFlags,
                0,
                shaderBlob.GetAddressOf(),
                errorBlob.GetAddressOf());

        if (FAILED(hr))
        {
            std::wstringstream stream;

            stream
                << L"HLSL Compile 실패\n"
                << L"File: "
                << path.wstring()
                << L"\nEntry: "
                << entryPoint
                << L"\nTarget: "
                << target
                << L"\nHRESULT: "
                << HResultToString(hr);

            const std::wstring compilerMessage =
                BlobTextToWide(
                    errorBlob.Get());

            if (!compilerMessage.empty())
            {
                stream
                    << L"\n\nCompiler Message:\n"
                    << compilerMessage;
            }

            outError =
                stream.str();

            return false;
        }

        return true;
    }
}

bool Shader::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& vertexShaderPath,
    const std::filesystem::path& pixelShaderPath)
{
    lastErrorMessage_.clear();

    if (!device)
    {
        lastErrorMessage_ =
            L"Shader::Initialize에 전달된 ID3D11Device가 nullptr입니다.";

        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;

    // ------------------------------------------------------------------------
    // Vertex Shader Compile
    // ------------------------------------------------------------------------
    if (!CompileShader(
            vertexShaderPath,
            "VSMain",
            "vs_5_0",
            vertexShaderBlob,
            lastErrorMessage_))
    {
        return false;
    }

    // ------------------------------------------------------------------------
    // Pixel Shader Compile
    // ------------------------------------------------------------------------
    if (!CompileShader(
            pixelShaderPath,
            "PSMain",
            "ps_5_0",
            pixelShaderBlob,
            lastErrorMessage_))
    {
        return false;
    }

    // ------------------------------------------------------------------------
    // Vertex Shader Object 생성
    // ------------------------------------------------------------------------
    HRESULT hr =
        device->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            nullptr,
            vertexShader_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"ID3D11Device::CreateVertexShader 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    // ------------------------------------------------------------------------
    // Pixel Shader Object 생성
    // ------------------------------------------------------------------------
    hr =
        device->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(),
            nullptr,
            pixelShader_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"ID3D11Device::CreatePixelShader 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    // C++ Vertex 구조체의 실제 메모리 Layout과 일치시킨다.
    const std::array<D3D11_INPUT_ELEMENT_DESC, 4>
        inputElements =
    {{
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            0,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "NORMAL",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            12,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            24,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            32,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        }
    }};

    // Vertex Shader Signature와 C++ Vertex Layout이 호환되는지 검증하면서
    // Input Layout Object를 생성한다.
    hr =
        device->CreateInputLayout(
            inputElements.data(),
            static_cast<UINT>(
                inputElements.size()),
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            inputLayout_.GetAddressOf());

    if (FAILED(hr))
    {
        lastErrorMessage_ =
            L"ID3D11Device::CreateInputLayout 실패\nHRESULT: " +
            HResultToString(hr);

        return false;
    }

    return true;
}

void Shader::Bind(
    ID3D11DeviceContext* context) const
{
    context->IASetInputLayout(
        inputLayout_.Get());

    context->VSSetShader(
        vertexShader_.Get(),
        nullptr,
        0);

    context->PSSetShader(
        pixelShader_.Get(),
        nullptr,
        0);
}

const std::wstring& Shader::GetLastErrorMessage() const
{
    return lastErrorMessage_;
}
