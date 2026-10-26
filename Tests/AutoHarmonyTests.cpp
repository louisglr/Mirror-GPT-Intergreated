#include "AutoHarmonyPlanner.h"
#include <cstdlib>
#include <iostream>

namespace
{
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
int pc(int n) { return ((n % 12) + 12) % 12; }

void musicalInvariants()
{
    constexpr int major[] = {0,2,4,5,7,9,11};
    constexpr int minor[] = {0,2,3,5,7,8,10};
    for (int root = 0; root < 12; ++root)
        for (int scale = 1; scale <= 2; ++scale)
            for (int voicing = 0; voicing < 3; ++voicing)
                for (int bits = 1; bits < 16; ++bits)
                {
                    AutoHarmonyPlanner p;
                    AutoHarmonyPlanner::Mask mask {};
                    for (int i=0; i<4; ++i) mask[(size_t)i] = (bits & (1<<i)) != 0;
                    const int* steps = scale == 2 ? minor : major;
                    // Include a low horn and a high flute register, then all
                    // seven scale degrees. No voice crossings or duplicates.
                    for (int octave : { 36, 60, 84 })
                        for (int degree=0; degree<7; ++degree)
                        {
                            const int lead = octave + root + steps[degree];
                            for (int frame=0; frame<10; ++frame)
                                p.update(lead, 0.95f, 0.01, root, scale, voicing, 1, mask);
                            check(p.isReady(), "valid diatonic melody produced no harmony");
                            int previous = -1;
                            for (int i=0; i<4; ++i)
                            {
                                const int n = p.getNotes()[(size_t)i];
                                if (!mask[(size_t)i]) { check(n == -1, "disabled voice consumed a note"); continue; }
                                check(n > previous && n >= 0 && n <= 127, "crossed, duplicated or invalid voice");
                                check(n != lead && std::abs(n-lead) <= 12, "lead collision or excessive pitch shift");
                                const int relative = pc(n-root);
                                check(std::find(steps, steps+7, relative) != steps+7, "harmony outside key");
                                previous = n;
                            }
                        }
                }
}

void continuityAndReset()
{
    AutoHarmonyPlanner p;
    AutoHarmonyPlanner::Mask all {true,true,true,true};
    p.update(60, 0.95f, 0.01, 0, 1, 1, 0, all);
    const auto first = p.getNotes();
    for(int i=0;i<100;++i) p.update(60, 0.95f, 0.01, 0, 1, 1, 0, all);
    check(p.getNotes() == first, "held melody wandered");
    for(int i=0;i<3;++i) p.update(62, 0.95f, 0.01, 0, 1, 1, 0, all);
    check(p.getNotes() == first, "brief passing note changed Calm harmony");
    p.update(60, 0.95f, 0.01, 0, 1, 1, 0, all);
    p.update(67, 0.1f, 0.01, 0, 1, 1, 0, all);
    check(p.getNotes() == first, "low-confidence pitch changed chord");
    for(int i=0;i<60;++i) p.update(60, 0.0f, 0.01, 0, 1, 1, 0, all);
    check(!p.isReady(), "silence retained an old phrase");
    p.update(64, 0.95f, 0.01, 0, 1, 1, 0, all);
    check(p.isReady(), "new phrase failed to re-enter");
    p.update(64, 0.95f, 0.01, 0, 1, 1, 0, {true,false,false,true});
    check(p.getNotes()[1] == -1 && p.getNotes()[2] == -1, "mask change did not revoice");
    p.reset();
    check(!p.isReady() && p.getChordDegree() == -1, "reset retained chord");
    p.update(61, 0.95f, 0.01, 0, 0, 1, 1, all);
    check(p.isReady() && p.getChordDegree() == -1, "chromatic fallback assumed a major key");
}
}
int main()
{
    musicalInvariants(); continuityAndReset();
    std::cout << (failures ? "Auto harmony tests FAILED\n" : "Auto harmony tests passed\n");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
