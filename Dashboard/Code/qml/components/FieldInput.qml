import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property alias text: field.text
    property alias placeholder: field.placeholderText
    property bool password: false
    property alias input: field
    signal submitted()

    implicitHeight: root.label.length ? 58 : 36
    implicitWidth: 280

    Text {
        id: caption
        visible: root.label.length > 0
        height: visible ? implicitHeight : 0
        text: root.label
        color: field.activeFocus ? Theme.text : Theme.textMuted
        font.family: App.uiFont
        font.pixelSize: 11
        font.weight: Font.Medium
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    Rectangle {
        anchors.top: caption.visible ? caption.bottom : parent.top
        anchors.topMargin: caption.visible ? 6 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        height: 36
        radius: Theme.radiusCtl
        color: Theme.bgInset
        border.width: 1
        border.color: field.activeFocus ? Theme.action : Theme.border
        Behavior on border.color { ColorAnimation { duration: 120 } }

        TextField {
            id: field
            anchors.fill: parent
            anchors.leftMargin: 11
            anchors.rightMargin: 11
            verticalAlignment: Text.AlignVCenter
            color: Theme.text
            font.family: root.password ? App.monoFont : App.uiFont
            font.pixelSize: 13
            echoMode: root.password ? TextInput.Password : TextInput.Normal
            selectByMouse: true
            placeholderTextColor: Theme.textFaint
            background: Item {}
            onAccepted: root.submitted()
        }
    }
}
