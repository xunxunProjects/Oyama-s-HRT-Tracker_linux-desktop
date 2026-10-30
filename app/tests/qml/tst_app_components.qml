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
