import QtQuick

Item {
    id: root
    property int pageIndex: 0
    property bool selected: App.page === pageIndex
    property string title: ""
    property string icon: "dashboard"
    property bool expanded: true
    signal clicked()

    height: 40
    width: parent ? parent.width : 200

    Rectangle {
        id: bg
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: Theme.radiusCtl
        color: {
            if (root.selected)
                return Theme.bgCard
            if (mouse.containsMouse)
                return Theme.bgInset
            return "transparent"
        }
        Behavior on color { ColorAnimation { duration: 100 } }

        Rectangle {
            visible: root.selected
            width: 2
            height: 16
            radius: 1
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            color: Theme.action
        }
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        // Keeps the glyph optically centred in the 72px rail, then slides left
        // as the panel opens instead of jumping.
        anchors.leftMargin: root.expanded ? 20 : 23
        Behavior on anchors.leftMargin { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        spacing: 14

        IconGlyph {
            width: 18
            height: 18
            name: root.icon
            stroke: root.selected || mouse.containsMouse ? Theme.text : Theme.textMuted
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.title
            color: root.selected || mouse.containsMouse ? Theme.text : Theme.textMuted
            font.family: App.uiFont
            font.pixelSize: 13
            font.weight: root.selected ? Font.DemiBold : Font.Normal
            anchors.verticalCenter: parent.verticalCenter
            opacity: root.expanded ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 140 } }
            visible: opacity > 0.02
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTipLite {
        show: mouse.containsMouse && !root.expanded
        text: root.title
    }
}
