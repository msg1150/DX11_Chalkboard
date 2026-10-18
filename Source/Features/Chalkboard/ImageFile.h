// [칠판 프로젝트에서의 역할]
// 합성된 RGBA 픽셀을 투명 PNG로 내보내는 API입니다.
// PNG에는 레이어 편집 구조가 없으므로 .chalk 저장과 용도가 다릅니다.

#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace Chalkboard
{
bool WritePng(const std::filesystem::path& path,std::uint32_t width,std::uint32_t height,
              const std::vector<std::uint32_t>& rgba,std::wstring& error);
}
