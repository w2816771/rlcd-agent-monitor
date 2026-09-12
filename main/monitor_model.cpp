#include "monitor_model.h"

MonitorModel monitor_demo_model() {
    return {
        true, false, 0, 82, 37, 70, 21, 8, 22.4f, 32.0f, 0.6f, 52.0f, 28.6f,
        24.6f, 47, 38, 184, 0.0f, 0.0f,
        "09:41", "SAT 12 SEP", 29, "PARTLY CLOUDY", "",
        {"CODEX", true, 186, 42, 8, 2.84f, "5H 1%", "7D 20%"},
        {"CLAUDE", false, 94, 67, 4, 1.92f, "5H 0", "7D 1.4M"},
        {{{"codex worker", 24.0f, 1843},
          {"claude", 18.0f, 1229},
          {"rust-analyzer", 11.0f, 846},
          {"codex app", 8.0f, 622},
          {"claude helper", 4.0f, 380}}},
        {{"New models push efficient on-device AI forward",
          "Developers rethink how coding agents work together",
          "Chip makers announce lower-power processors",
          "Open-source tools gain new enterprise features",
          "AI hardware efficiency reaches a new milestone",
          "Cloud provider cuts inference pricing again",
          "Open model tooling adds local deployment support",
          "Researchers publish agent safety benchmark"}},
        {{"22m", "48m", "1h", "2h", "3h", "4h", "5h", "6h"}}
    };
}
