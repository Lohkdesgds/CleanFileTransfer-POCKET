#include "display.h"

void gotoxy(const short x, const short y)
{
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD{x,y});
}

void getcmdsize(int& w, int& h)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}

void echokb(const bool ech)
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, (mode & (~ENABLE_ECHO_INPUT)) | (ech ? ENABLE_ECHO_INPUT : 0));

    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cci;
    GetConsoleCursorInfo(hStdout, &cci);
    cci.bVisible = ech; // show/hide cursor
    SetConsoleCursorInfo(hStdout, &cci);
}

std::string slice_by_time(const std::string& orig, const size_t maxlen)
{
    if (orig.size() <= maxlen) return orig;
    constexpr size_t off_before = 10, off_after = 10;
    const size_t to_slice_off = orig.size() - maxlen; // by check before, always true.

    const auto time_sec = std::chrono::duration_cast<std::chrono::duration<unsigned long long, std::ratio<1, 10>>>(std::chrono::system_clock::now().time_since_epoch()).count();

    const size_t timu = time_sec % (to_slice_off + off_before + off_after);

    if (timu < off_before) {
        return orig.substr(0, maxlen);
    }
    if (timu < (off_before + to_slice_off)) {
        return orig.substr(timu - off_before, maxlen);
    }
    return orig.substr(orig.size() - maxlen);
}

void DisplayCMD::draw()
{
    auto last_cleanup = std::chrono::system_clock::now();
    auto time_for_cleanup = [&last_cleanup] { if (std::chrono::system_clock::now() >= last_cleanup) { last_cleanup = std::chrono::system_clock::now() + std::chrono::seconds(2); return true; } return false; };

    while (keep) {

        gotoxy(0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        const bool must_clean = time_for_cleanup();

        if (must_clean) echokb(false);

        int w, h;
        getcmdsize(w, h);
        if (w < 4 || h < 5) {
            gotoxy(0, 0);
            printf_s("FATAL ERROR, SMALL CONSOLE!");
            continue;
        }

        if (must_clean) {
            for (int u = 0; u < w; ++u) putchar('#');
            gotoxy(0, 1);
            {
                putchar('#');
                const auto mstr = slice_by_time(gen_top(), w - 2);

                int pstart = ((w - static_cast<int>(mstr.length())) / 2) - 1;
                if (pstart < 0 || pstart >(w - 2)) pstart = 0;

                for (int u = 0; u < pstart; ++u) putchar(' ');

                printf_s("%*s", -(w - 2 - pstart), mstr.c_str());
                putchar('#');
            }
            gotoxy(0, 2);
            for (int u = 0; u < w; ++u) putchar('#');
        }
        else {
            const auto mstr = slice_by_time(gen_top(), w - 2);

            int pstart = ((w - static_cast<int>(mstr.length())) / 2) - 1;
            if (pstart < 0 || pstart >(w - 2)) pstart = 0;

            gotoxy(1 + pstart, 1);
            printf_s("%*s", -(w - 2 - pstart), mstr.c_str());
        }


        std::lock_guard<std::recursive_mutex> luck(saf);
        for (int lns = h - 3; lns >= 3; --lns)
        {
            gotoxy(0, lns);

            const int off = (h - 3) - (lns);

            if (off < lines.size()) {
                const auto& it = (lines.rbegin() + off);
                if (must_clean) printf_s("%*s", -w, slice_by_time(*it, w).c_str());
                else printf_s("%*s", -w, slice_by_time(*it, w).c_str());
            }
            else if (must_clean) {
                for (int u = 0; u < w; ++u) putchar(' ');
            }
        }

        if (must_clean) {
            gotoxy(0, h - 2);
            for (int u = 0; u < w; ++u) putchar('#');
        }
        gotoxy(0, h - 1);
        printf_s("> %*s", -(w - 2), curr_input.substr((curr_input.size() > (w - 2) ? (curr_input.size() - (w - 2)) : 0), w - 2).c_str());
    }
}

void DisplayCMD::cread()
{
    auto last_now = std::chrono::system_clock::now();

    while (keep)
    {
        if (_kbhit())
        {
            int _inn = _getch();
            switch (_inn) {
            case 13:
            {
                const std::string cpycmd = curr_input;
                {
                    std::lock_guard<std::recursive_mutex> luck(saf);
                    curr_input.clear();
                }
                on_in(cpycmd);
            }
                break;
            case 8:
            {
                std::lock_guard<std::recursive_mutex> luck(saf);
                if (curr_input.size()) curr_input.pop_back();
            }
                break;
            default:
            {
                std::lock_guard<std::recursive_mutex> luck(saf);
                if (curr_input.size() < max_cmd_len && _inn >= 32 && _inn != 127 && _inn <= 255) curr_input += _inn;
            }
                break;
            }
            last_now = std::chrono::system_clock::now() + std::chrono::seconds(2);
        }
        else {
            if (last_now < std::chrono::system_clock::now()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            //else std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

DisplayCMD::DisplayCMD(const std::function<std::string(void)>& f, const std::function<void(const std::string&)>& i)
    : gen_top(f), on_in(i)
{
    if (!gen_top || !on_in) throw std::invalid_argument("Null or empty arguments.");
    keep = true;
    thr = std::thread([this] {cread(); });
    //thr2 = std::thread([this] {cread(); });
}

DisplayCMD::~DisplayCMD()
{
    keep = false;
    if (thr.joinable()) thr.join();
    //if (thr2.joinable()) thr2.join();
}

void DisplayCMD::push_message(const std::string& s)
{
    std::lock_guard<std::recursive_mutex> luck(saf);
    lines.push_back(s.substr(0, max_buf_eachline_len));
    if (lines.size() > max_buf_lines) lines.erase(lines.begin());
}

void DisplayCMD::set_end()
{
    keep = false;
}

void DisplayCMD::yield_draw()
{
    while (keep) draw();
}
