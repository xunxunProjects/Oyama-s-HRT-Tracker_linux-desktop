import QtQuick
import QtTest
import OyamaHrt 1.0

TestCase {
    id: testCase
    name: "AppComponents"
    when: windowShown
    width: 480
    height: 400

    AppComboBox {
        id: combo
        x: 20
        y: 20
        width: 240
        textRole: "label"
        valueRole: "value"
        model: [
            { value: "oral", label: "Oral" },
            { value: "sublingual", label: "Sublingual" },
            { value: "injection", label: "Injection" }
        ]
    }

    function test_curvePointIndexAt() {
        const points = [
            { t: 0, e2: 0 },
            { t: 10, e2: 20 },
            { t: 20, e2: 5 }
        ]
        compare(CurveMath.pointIndexAt(points, 0, 48, 100), -1)
        compare(CurveMath.pointIndexAt(points, 48, 48, 100), 0)
        compare(CurveMath.pointIndexAt(points, 98, 48, 100), 1)
        compare(CurveMath.pointIndexAt(points, 148, 48, 100), 2)
        compare(CurveMath.pointIndexAt(points, 47, 48, 100), -1)
        compare(CurveMath.pointIndexAt(points, 149, 48, 100), -1)
        compare(CurveMath.pointIndexAt(points, 100, 48, 0), -1)
        compare(CurveMath.pointIndexAt([{ t: 0, e2: 1 }], 48, 48, 100), -1)
    }

    function test_popupShowsDelegateLabels() {
        combo.popup.open()
        tryVerify(function() { return combo.popup.visible })
        wait(50)

        const list = combo.popup.contentItem
        compare(list.count, combo.model.length)
        for (let i = 0; i < combo.model.length; ++i) {
            const item = list.itemAtIndex(i)
            verify(item, "delegate " + i + " was not created")
            compare(item.text, combo.model[i].label)
        }

        combo.popup.close()
    }
}
