#include "app_controller.h"

#include <QCoreApplication>
#include <QString>
#include <QVariantMap>

#include <cmath>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << "app_controller_test: " << message << '\n';
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);

    AppController controller;
    if (!controller.status().startsWith("Connected")) {
        std::cerr << "unexpected status: "
                  << controller.status().toStdString() << '\n';
        return fail("controller did not connect to the test database");
    }
    if (std::abs(controller.weight() - 70.0) > 1e-9) {
        return fail("unset weight should fall back to 70 kg");
    }
    if (controller.birthYear() != 0) {
        return fail("unset birth year should be 0");
    }

    if (!controller.saveSettings(68.5, 1991, "mipd", "retrospective",
                                 R"({"e2_kClear":0.4})")) {
        std::cerr << "saveSettings failed: "
                  << controller.status().toStdString() << '\n';
        return 1;
    }
    if (std::abs(controller.weight() - 68.5) > 1e-9 ||
        controller.birthYear() != 1991 ||
        controller.calibrationMethod() != "mipd" ||
        controller.calibrationHistory() != "retrospective" ||
        !controller.pkParamsJson().contains("e2_kClear")) {
        return fail("settings did not round-trip");
    }

    const double now = controller.nowHours();
    if (!controller.addDose("oral", "E2", 2.0, now - 24.0, {})) {
        std::cerr << "addDose failed: "
                  << controller.status().toStdString() << '\n';
        return 1;
    }
    if (controller.doses().size() != 1) {
        return fail("expected exactly one dose");
    }
    const QVariantMap added = controller.doses().front().toMap();
    const QString id = added.value("id").toString();
    if (id.isEmpty() || added.value("route").toString() != "oral" ||
        added.value("ester").toString() != "E2") {
        return fail("added dose has unexpected fields");
    }
    if (controller.curve().isEmpty()) {
        return fail("curve should not be empty after adding a dose");
    }

    QVariantMap extras;
    extras.insert("sublingualTier", 2);
    if (!controller.updateDose(id, "sublingual", "EV", 4.0, now - 24.0,
                               extras)) {
        std::cerr << "updateDose failed: "
                  << controller.status().toStdString() << '\n';
        return 1;
    }
    if (controller.doses().size() != 1) {
        return fail("editing must not create a second dose");
    }
    const QVariantMap updated = controller.doses().front().toMap();
    if (updated.value("route").toString() != "sublingual" ||
        updated.value("ester").toString() != "EV" ||
        std::abs(updated.value("doseMG").toDouble() - 4.0) > 1e-9 ||
        !updated.value("extras").toString().contains("sublingualTier")) {
        return fail("updated dose did not round-trip");
    }

    if (!controller.removeDose(id)) {
        std::cerr << "removeDose failed: "
                  << controller.status().toStdString() << '\n';
        return 1;
    }
    if (!controller.doses().isEmpty()) {
        return fail("dose was not removed");
    }

    std::cout << "app_controller_test: ok\n";
    return 0;
}
