#include <picross/pic_usb_iso_out.h>
#include <array>
#include <iostream>

int main()
{
    // A wrapped ring whose requests are all submitted/pending, as observed on Tau.
    const std::array<pic::detail::iso_out_span, 3> pending {{ {104, true}, {108, true}, {100, true} }};
    unsigned reads = 0;
    const auto find = [&](bool running, uint64_t target) {
        reads = 0;
        return pic::detail::scheduled_iso_out_frame(running, target, 3, 4,
            [&](unsigned urb) { ++reads; return pending[urb]; });
    };
    bool ok = true;
    ok &= find(false, 104) == 12 && reads == 0; // Startup: no access to uninitialised URBs.
    ok &= find(true, 104) == 0;
    ok &= find(true, 107) == 3;
    ok &= find(true, 108) == 4;
    ok &= find(true, 100) == 8; // Physical ring wrap is independent of bus-frame order.
    ok &= find(true, 99) == 12 && reads == 3;
    ok &= find(true, 112) == 12 && reads == 3; // Outside submitted horizon: bounded failure.
    ok &= pic::detail::scheduled_iso_out_frame(true, 104, 3, 4,
        [](unsigned) { return pic::detail::iso_out_span {104, false}; }) == 12;
    const auto first = pic::detail::next_iso_out_frame(100, 0, 4);
    ok &= first == 104;
    const auto second = pic::detail::next_iso_out_frame(100, first + 7, 4);
    ok &= second == 111; // Two 512-frame writes in one callback must not overwrite.
    ok &= pic::detail::next_iso_out_frame(120, second + 7, 4) == 124; // Catch up after a gap.
    // When a request is recycled, its old schedule must no longer be selected.
    auto recycled = pending;
    recycled[0] = {116, true};
    ok &= pic::detail::scheduled_iso_out_frame(true, 104, 3, 4,
        [&](unsigned urb) { return recycled[urb]; }) == 12;
    ok &= pic::detail::scheduled_iso_out_frame(true, 116, 3, 4,
        [&](unsigned urb) { return recycled[urb]; }) == 0;
    if (!ok) { std::cerr << "macOS USB output scheduling checks failed\n"; return 1; }
    std::cout << "macOS USB output scheduling checks passed\n";
}
