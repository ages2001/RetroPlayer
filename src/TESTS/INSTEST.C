/* Regression for the self-extracting installer: build a payload blob in a
 * private file, extract it and verify the files, plus unit checks on the name
 * filter and the per-OS destination. Never installs into a system location. */
#define RP_INSTALL_TEST
#include "../SRC/INSTALL.C"

static unsigned checks, failures, first_failure;
static HANDLE report;
#define CHECK(x) do{++checks;if(!(x)){++failures;if(!first_failure)first_failure=__LINE__;}}while(0)
#define REQUIRE(x) do{CHECK(x);if(failures)goto finish;}while(0)

static int compare_w(const WCHAR *a, const WCHAR *b) { while (*a && *a == *b) { ++a; ++b; } return *a - *b; }
static BOOL ends_with_w(const WCHAR *s, const WCHAR *suf){ unsigned ls=length(s),lf=length(suf); return lf<=ls && !compare_w(s+ls-lf,suf); }

static DWORD crc_of(const char *data, DWORD n)
{
    DWORD crc = 0xffffffffUL, i;
    for (i = 0; i < n; ++i) crc = (crc>>8) ^ crc_table[(crc ^ (unsigned char)data[i]) & 255];
    return crc ^ 0xffffffffUL;
}
static void put_u16(HANDLE f, DWORD v) { unsigned char b[2]; DWORD w; b[0]=v&255; b[1]=(v>>8)&255; WriteFile(f,b,2,&w,NULL); }
static void put_u32(HANDLE f, DWORD v) { unsigned char b[4]; DWORD w; b[0]=v&255; b[1]=(v>>8)&255; b[2]=(v>>16)&255; b[3]=(v>>24)&255; WriteFile(f,b,4,&w,NULL); }
static void put_entry(HANDLE f, const char *name, const char *data, DWORD n)
{
    DWORD w;
    put_u16(f, lstrlenA(name)); WriteFile(f, name, lstrlenA(name), &w, NULL);
    put_u32(f, n); put_u32(f, crc_of(data, n)); WriteFile(f, data, n, &w, NULL);
}
static BOOL file_is(const WCHAR *path, const char *expect)
{
    char buf[64]; DWORD got; BOOL ok;
    HANDLE f = open_file(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(f, buf, sizeof(buf)-1, &got, NULL);
    CloseHandle(f);
    if (!ok) return FALSE;
    buf[got] = 0;
    return !lstrcmpA(buf, expect);
}

void WINAPI RPEntry(void)
{
    char report_path[MAX_PATH], tmpdir[MAX_PATH], blobname[MAX_PATH], *last;
    WCHAR rootw[MAX_PATH], blobw[MAX_PATH], path[MAX_PATH];
    DWORD size, written, payload_start, filesize;
    HANDLE blob;
    nt_system = !(GetVersion() & 0x80000000UL);
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    size = GetModuleFileNameA(NULL, report_path, MAX_PATH); if (!size || size >= MAX_PATH-16) ExitProcess(2);
    last = report_path; { char *p; for (p = report_path; *p; ++p) if (*p=='\\') last = p+1; }
    lstrcpyA(last, "INSTEST.TXT");
    report = CreateFileA(report_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (report == INVALID_HANDLE_VALUE) ExitProcess(3);
    crc_init();

    /* name filter */
    { const char *bad[] = { "", "..\\A.TXT", "A/FILE.TXT", "C:\\A.TXT", "A\\..\\B.TXT",
        "CON.TXT", "PRN", "AUX.BIN", "NUL", "LONGERTHAN8.TXT", "A.LONG", "A\\", "\\A.TXT" };
      unsigned i; for (i=0;i<sizeof(bad)/sizeof(bad[0]);++i) CHECK(!safe_name(bad[i])); }
    CHECK(safe_name("RETROPLY.EXE")); CHECK(safe_name("PLUGINS\\P1234567.DLL"));
    CHECK(safe_name("LICENSES\\VLCGPL.TXT"));

    /* per-OS destination */
    { WCHAR t[MAX_PATH]; BOOL saved = nt_system;
      nt_system = TRUE;
      compute_target(0x00000A03, t); CHECK(!compare_w(t, L"C:\\RETROPLY"));   /* NT 3.10 */
      compute_target(0x00003303, t); CHECK(!compare_w(t, L"C:\\RETROPLY"));   /* NT 3.51 */
      compute_target(0x00000004, t); CHECK(ends_with_w(t, L"\\Retro Player")); /* NT 4   */
      nt_system = FALSE;
      compute_target(0x80000004, t); CHECK(ends_with_w(t, L"\\Retro Player")); /* 9x     */
      nt_system = saved;
    }

    /* build a payload blob in a private directory next to the report */
    lstrcpyA(tmpdir, report_path); { char *p; last = tmpdir; for (p = tmpdir; *p; ++p) if (*p=='\\') last = p+1; }
    *last = 0;
    REQUIRE(tmpdir[0] != 0);
    REQUIRE(GetTempFileNameA(tmpdir, "RPB", 0, blobname) != 0);
    REQUIRE(DeleteFileA(blobname));
    REQUIRE(CreateDirectoryA(blobname, NULL));           /* becomes the extract target */
    REQUIRE(MultiByteToWideChar(CP_ACP,0,blobname,-1,rootw,MAX_PATH) > 0);
    copy_w(target_dir, rootw);
    lstrcpyA(blobname+lstrlenA(blobname), "\\PAY.BIN");
    REQUIRE(MultiByteToWideChar(CP_ACP,0,blobname,-1,blobw,MAX_PATH) > 0);

    blob = CreateFileA(blobname, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(blob != INVALID_HANDLE_VALUE);
    payload_start = 0;                                   /* blob file holds only the payload */
    put_entry(blob, "A.TXT", "hello", 5);
    put_entry(blob, "PLUGINS\\B.BIN", "payload-bytes", 13);
    { DWORD hi=0; filesize = GetFileSize(blob, &hi); }
    put_u32(blob, filesize - payload_start);             /* payload size */
    WriteFile(blob, "RPSX", 4, &written, NULL);
    CloseHandle(blob);

    /* extract */
    self = open_file(blobw, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
    REQUIRE(self != INVALID_HANDLE_VALUE);
    { DWORD hi=0; filesize = GetFileSize(self, &hi); }
    CHECK(extract(payload_start, filesize - 8));
    CloseHandle(self);
    CHECK(join(path, target_dir, "A.TXT")); CHECK(file_is(path, "hello"));
    CHECK(join(path, target_dir, "PLUGINS\\B.BIN")); CHECK(file_is(path, "payload-bytes"));

    /* a corrupt CRC must be rejected and roll back */
    blob = open_file(blobw, GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING);
    if (blob != INVALID_HANDLE_VALUE) {
        DWORD w; SetFilePointer(blob, 20, NULL, FILE_BEGIN); WriteFile(blob, "X", 1, &w, NULL); CloseHandle(blob);
    }
    { WCHAR p2[MAX_PATH]; if (join(p2, target_dir, "A.TXT")) remove_file(p2);
      if (join(p2, target_dir, "PLUGINS\\B.BIN")) remove_file(p2); }
    self = open_file(blobw, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
    if (self != INVALID_HANDLE_VALUE) {
        DWORD hi=0; filesize = GetFileSize(self, &hi);
        CHECK(!extract(payload_start, filesize - 8));    /* CRC mismatch -> fail */
        undo();
        CloseHandle(self);
    }

    /* cleanup */
    { WCHAR p2[MAX_PATH];
      if (join(p2, target_dir, "A.TXT")) remove_file(p2);
      if (join(p2, target_dir, "PLUGINS\\B.BIN")) remove_file(p2);
      if (join(p2, target_dir, "PLUGINS")) { char aa[MAX_PATH]; if (ansi(p2,aa)) RemoveDirectoryA(aa); }
      remove_file(blobw);
      { char aa[MAX_PATH]; if (ansi(target_dir,aa)) RemoveDirectoryA(aa); }
    }
finish:
    { char line[160]; DWORD w;
      wsprintfA(line, "CHECKS=%u FAILURES=%u FIRST_FAIL=%u\r\nRESULT=%s\r\n",
                checks, failures, first_failure, failures ? "FAIL" : "PASS");
      WriteFile(report, line, lstrlenA(line), &w, NULL); CloseHandle(report); }
    ExitProcess(failures ? 1 : 0);
}
