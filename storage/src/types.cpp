#include <oyama/storage/types.h>

#include <stdexcept>

namespace oyama::storage {

std::string to_string(Mode mode) {
    switch (mode) {
        case Mode::Transfem:
            return "transfem";
        case Mode::Transmasc:
            return "transmasc";
    }
    throw std::invalid_argument("unknown HRT mode");
}

std::optional<Mode> parse_mode(std::string_view text) {
    if (text == "transfem") {
        return Mode::Transfem;
    }
    if (text == "transmasc") {
        return Mode::Transmasc;
    }
    return std::nullopt;
}

std::string table_name(JsonRecordKind kind) {
    switch (kind) {
        case JsonRecordKind::DoseTemplate:
            return "dose_templates";
        case JsonRecordKind::QuickDose:
            return "quick_doses";
        case JsonRecordKind::Supply:
            return "supplies";
        case JsonRecordKind::PlanItem:
            return "plan_items";
    }
    throw std::invalid_argument("unknown JSON record kind");
}

}  // namespace oyama::storage
