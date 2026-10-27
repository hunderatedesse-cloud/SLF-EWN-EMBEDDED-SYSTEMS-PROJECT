import QtQuick

Rectangle {
    id: root
    property string text: ""
    property color tone: Theme.safe

    implicitWidth: lab.implicitWidth + 16
    implicitHeight: 22
    radius: 11
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.15)
    border.width: 0

    Text {
        id: lab
        anchors.centerIn: parent
        text: root.text
        color: root.tone
        font.family: App.uiFont
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
