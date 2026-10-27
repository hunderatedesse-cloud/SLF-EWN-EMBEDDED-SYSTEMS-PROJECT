import QtQuick

Item {
    id: root
    property bool primary: false
    property bool destructive: false
    property bool compact: false
    property alias text: label.text
    property bool hovered: mouse.containsMouse
    signal clicked()

    implicitWidth: Math.max(compact ? 72 : 96, label.implicitWidth + (compact ? 18 : 24))
    implicitHeight: compact ? 28 : 32
    opacity: enabled ? 1 : 0.4
    Behavior on opacity { NumberAnimation { duration: 120 } }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 8
        color: {
            if (root.primary)
                return mouse.containsMouse ? Qt.lighter(Theme.action, 1.08) : Theme.action
            if (mouse.containsMouse)
                return Theme.bgCardHover
            return root.destructive ? "transparent" : Theme.bgInset
        }
        border.width: 1
        border.color: {
            if (root.primary)
                return Qt.darker(Theme.action, 1.15)
            if (root.destructive)
                return mouse.containsMouse ? Theme.textDim : Theme.border
            return Theme.border
        }

        Behavior on color { ColorAnimation { duration: 90 } }
        Behavior on border.color { ColorAnimation { duration: 90 } }
    }

    Text {
        id: label
        anchors.centerIn: parent
        color: root.primary ? Theme.actionInk : Theme.text
        font.family: App.uiFont
        font.pixelSize: compact ? 12 : 13
        font.weight: root.primary ? Font.DemiBold : Font.Medium
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
