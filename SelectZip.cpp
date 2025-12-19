#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <zip.h> // libzip 헤더
#include <windows.h> // GetModuleFileName 사용을 위해 필요

namespace fs = std::filesystem;

// 문자열을 소문자로 변환 (확장자 비교용)
std::string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
}

// 구분자(;)로 확장자 목록 파싱
std::vector<std::string> parseExtensions(const std::string& extString) {
    std::vector<std::string> extensions;
    std::stringstream ss(extString);
    std::string segment;
    while (std::getline(ss, segment, ';')) {
        // 공백 제거 및 점(.)이 없으면 추가
        if (!segment.empty()) {
            if (segment[0] != '.') segment = "." + segment;
            extensions.push_back(toLower(segment));
        }
    }
    return extensions;
}

// ZIP 내부 경로 형식으로 변환 (Windows '\' -> ZIP '/')
std::string makeZipPath(const fs::path& root, const fs::path& filePath) {
    // root로부터의 상대 경로 계산
    fs::path relative = fs::relative(filePath, root);
    std::string genericPath = relative.generic_string(); // '/' 구분자로 변환
    return genericPath;
}

int main(int argc, char* argv[]) {
    // 1. 파라미터 검증
    if (argc < 4) {
        std::cout << "Usage: .\\SelectZip.exe <RootDir> <Extensions> <ZipFileName>" << std::endl;
        std::cout << "Example: .\\SelectZip.exe \"C:\\Data\" \"txt;cpp\" \"C:\\Backup\\data.zip\"" << std::endl;
        std::cout << "if <RootDir> is . then use current directory." << std::endl;
        std::cout << "if <ZipFileName> is . then use <current directory name>.zip" << std::endl;
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::string inputRoot = argv[1];
    fs::path rootDir;

    if (inputRoot == ".") {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);

        rootDir = fs::path(path).parent_path();
        std::cout << "[Info] '.' detected. Setting root to executable path: " << rootDir << std::endl;
    }
    else {
        rootDir = inputRoot;
    }

    std::string extListStr = argv[2];

    //fs::path zipFilePath = argv[3];
    std::string inputZipName = argv[3];
    fs::path zipFilePath;

    // [로직 2] ZipFileName 처리: . 입력 시 (디렉토리명).zip 사용
    if (inputZipName == ".") {
        // rootDir의 마지막 폴더명을 가져와 .zip을 붙임
        // 예: C:\Data\Project -> Project.zip
        std::string dirName = rootDir.filename().string();
        if (dirName.empty()) { // 루트 디렉토리(C:\ 등)일 경우 예외 처리
            dirName = "Archive";
        }
        zipFilePath = rootDir / (dirName + ".zip");
    }
    else {
        zipFilePath = inputZipName;
        // 압축파일명에 경로가 없으면 루트 디렉토리에 생성
        if (!zipFilePath.has_parent_path()) {
            zipFilePath = rootDir / zipFilePath;
        }
    }

    // 6. 압축파일명에 경로가 없으면 루트 디렉토리에 생성
    if (!zipFilePath.has_parent_path()) {
        zipFilePath = rootDir / zipFilePath;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "Root Directory : " << rootDir << std::endl;
    std::cout << "Target Extensions: " << extListStr << std::endl;
    std::cout << "Output Zip File  : " << zipFilePath << std::endl;
    std::cout << "========================================" << std::endl;

    // 확장자 목록 파싱
    auto targetExtensions = parseExtensions(extListStr);

    // libzip 변수
    int errorp = 0;
    zip_t* archive = nullptr;

    // 2. ZIP 파일 열기 (존재하면 열고, 없으면 생성)
    // ZIP_CREATE: 없으면 생성, 일반적인 open은 기존 파일 유지 (업데이트 모드)
    archive = zip_open(zipFilePath.string().c_str(), ZIP_CREATE, &errorp);

    if (!archive) {
        zip_error_t ziperror;
        zip_error_init_with_code(&ziperror, errorp);
        std::cerr << "Failed to open output file: " << zip_error_strerror(&ziperror) << std::endl;
        zip_error_fini(&ziperror);
        return 1;
    }

    int processedCount = 0;

    try {
        // 1. 지정된 루트 디렉토리 순회 (재귀적)
        if (fs::exists(rootDir) && fs::is_directory(rootDir)) {
            for (const auto& entry : fs::recursive_directory_iterator(rootDir)) {
                if (entry.is_regular_file()) {
                    std::string fileExt = toLower(entry.path().extension().string());

                    // 확장자 확인
                    bool match = false;
                    for (const auto& ext : targetExtensions) {
                        if (fileExt == ext) {
                            match = true;
                            break;
                        }
                    }

                    if (match) {
                        // ZIP 내부 경로 생성
                        std::string entryName = makeZipPath(rootDir, entry.path());
                        std::string fullPathStr = entry.path().string();

                        // 3. 콘솔 표시
                        std::cout << "Processing: " << entryName << std::endl;

                        // 파일 소스 생성
                        zip_source_t* source = zip_source_file(archive, fullPathStr.c_str(), 0, 0);
                        if (source == nullptr) {
                            std::cerr << "  [Error] Failed to create source for: " << fullPathStr << std::endl;
                            continue;
                        }

                        // 2. 파일 추가 또는 업데이트
                        // 파일이 이미 존재하는지 확인
                        zip_int64_t index = zip_name_locate(archive, entryName.c_str(), 0);

                        if (index >= 0) {
                            // 존재하면 교체 (Update)
                            if (zip_file_replace(archive, index, source, 0) < 0) {
                                std::cerr << "  [Error] Failed to replace file: " << zip_strerror(archive) << std::endl;
                                zip_source_free(source);
                            }
                            else {
                                std::cout << "  -> [Updated]" << std::endl;
                            }
                        }
                        else {
                            // 없으면 추가 (Add)
                            if (zip_file_add(archive, entryName.c_str(), source, 0) < 0) {
                                std::cerr << "  [Error] Failed to add file: " << zip_strerror(archive) << std::endl;
                                zip_source_free(source);
                            }
                            else {
                                std::cout << "  -> [Added]" << std::endl;
                            }
                        }
                        processedCount++;
                    }
                }
            }
        }
        else {
            std::cerr << "Invalid root directory path." << std::endl;
        }
    }
    catch (const fs::filesystem_error& ex) {
        std::cerr << "Filesystem error: " << ex.what() << std::endl;
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
    }

    // ZIP 닫기 (이 시점에 실제 쓰기 작업이 수행됨)
    std::cout << "Finalizing archive (this may take a moment)..." << std::endl;
    if (zip_close(archive) < 0) {
        std::cerr << "Failed to write zip file: " << zip_strerror(archive) << std::endl;
    }
    else {
        std::cout << "Done! Total " << processedCount << " files processed." << std::endl;
    }

    // 4. 사용자 입력 대기
    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}
