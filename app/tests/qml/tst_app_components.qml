import QtQuick
import QtTest
import OyamaHrt 1.0
import "../../qml/Doses.js" as Doses

TestCase {
    id: testCase
    name: "AppComponents"
    when: windowShown
    width: 480
    height: 600

    ChoiceField {
        id: choice
        x: 20
        y: 20
        width: 300
        label: "Route"
        model: [
            { value: "oral", label: "Oral" },
            { value: "sublingual", label: "Sublingual" },
            { value: "injection", label: "Injection" }
        ]
        currentValue: "oral"
    }

    SignalSpy {
        id: activated
        target: choice
        signalName: "activated"
    }

    InlinePrompt {
        id: prompt
        x: 20
        y: 400
        width: 400
        title: "Delete this record?"
        cancelText: "Keep it"
        confirmText: "Delete"
    }

    SignalSpy {
        id: confirmed
        target: prompt
        signalName: "confirmed"
    }

    SignalSpy {
        id: cancelled
        target: prompt
        signalName: "cancelled"
    }

    function findAll(item, predicate, out) {
        out = out || []
        if (predicate(item))
            out.push(item)
        for (let i = 0; i < item.children.length; ++i)
            findAll(item.children[i], predicate, out)
        return out
    }

    // Choices unfold in place, under the field, as rows with a tick; there
    // is no pop-up to open.
    function test_choiceUnfoldsInPlace() {
        const rows = findAll(choice, function(i) { return i.selected !== undefined && i.title !== undefined })
        compare(rows.length, choice.model.length)
        for (let i = 0; i < rows.length; ++i)
            compare(rows[i].title, choice.model[i].label)

        const closedHeight = choice.height
        choice.open = true
        tryVerify(function() { return choice.height > closedHeight + 100 })

        verify(rows[0].selected)
        mouseClick(rows[2])
        compare(choice.currentValue, "injection")
        compare(activated.count, 1)
        verify(!choice.open)
        tryVerify(function() { return Math.abs(choice.height - closedHeight) < 1 })
    }

    // The question's buttons answer it; Escape backs out.
    function test_inlinePromptAnswers() {
        const buttons = findAll(prompt, function(i) { return i.variant !== undefined })
        compare(buttons.length, 2)
        mouseClick(buttons[1])
        compare(confirmed.count, 1)
        mouseClick(buttons[0])
        compare(cancelled.count, 1)
        prompt.focusKeep()
        keyClick(Qt.Key_Escape)
        compare(cancelled.count, 2)
    }

    // What the dose form offers, as on the web.
    function test_doseChoices() {
        compare(Doses.routesFor(true), ["injection", "gel"])
        compare(Doses.estersFor("injection", false), ["EB", "EV", "EC", "EN", "EU"])
        compare(Doses.estersFor("oral", false), ["E2", "EV", "CPA"])
        compare(Doses.estersFor("injection", true), ["TC", "TE", "TU"])
        fuzzyCompare(Doses.factor("EV") * 4, 3.056, 0.001)
        compare(Doses.factor("CPA"), 1)
        compare(Doses.trim(12.5), "12.5")
    }
}
