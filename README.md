# SelectZip

https://iwoohaha.tistory.com/383

지정하는 디렉토리 하위의 지정 확장자 파일만을 압축하는 콘솔 유틸리티 프로그램

## 사용방법

**SelectZip** `<RootDir>` `<Extensions>` `<ZipFileName>`

### Example:

`SelectZip "C:\Data" "txt;cpp" "C:\Backup\data.zip"`

---

### 📌 사용 시 유의사항

* 각 파라미터는 **큰따옴표 (`" "`)** 로 묶어주는 것이 좋습니다.
* `<Extensions>`: 여러 확장자는 **세미콜론 (`;`)** 문자로 연결합니다.
* `<ZipFileName>`에 경로를 붙이지 않으면, `<RootDir>` 경로를 붙여서 사용하게 됩니다.
