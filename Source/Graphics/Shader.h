// [칠판 프로젝트에서의 역할]
// HLSL 컴파일 결과와 Vertex/Pixel Shader 및 입력 정점 레이아웃을 관리합니다.

// ============================================================================
// Shader.h
// ----------------------------------------------------------------------------
// HLSL Vertex / Pixel Shader와 Input Layout을 관리한다.
//
// v0.3:
// Shader 파일이 없는 경우와 HLSL 컴파일 실패를 구분하고,
// D3DCompiler가 제공하는 실제 오류 메시지를 저장한다.
// ============================================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

class Shader
{
public:
    bool Initialize(
        ID3D11Device* device,
        const std::filesystem::path& vertexShaderPath,
        const std::filesystem::path& pixelShaderPath);

    void Bind(
        ID3D11DeviceContext* context) const;

    // 마지막 Shader 초기화/컴파일 오류를 반환한다.
    const std::wstring& GetLastErrorMessage() const;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;

    std::wstring lastErrorMessage_;
};
