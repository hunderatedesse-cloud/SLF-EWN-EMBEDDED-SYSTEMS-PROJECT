import QtQuick

// Small label anchored to the left of its parent so it stays inside the
// sidebar's clip rect instead of being cut off at the panel edge.
Rectangle {
    id: tip
    property string text: ""
    property bool show: false

    x: -width - 8
    y: parent ? (parent.height - height) / 2 : 0
    width: label.implicitWidth + 16
    height: 22
    radius: 4
    z: 999
    color: Theme.bgInset
    border.color: Theme.border
    border.width: 1

    opacity: show ? 1 : 0
    visible: opacity > 0.02
    Behavior on opacity { NumberAnimation { duration: 120 } }

    Text {
        id: label
        anchors.centerIn: parent
        text: tip.text
        color: Theme.text
        font.family: App.uiFont
        font.pixelSize: 11
    }
}
