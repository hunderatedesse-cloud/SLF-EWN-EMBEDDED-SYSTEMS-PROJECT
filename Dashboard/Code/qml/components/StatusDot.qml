import QtQuick

Item {
    id: root
    property int linkState: 0
    property real pulse: 1
    width: 8
    height: 8

    readonly property color dotColor: {
        switch (linkState) {
        case 1: return Theme.action
        case 2: return Theme.safe
        case 3: return Theme.critical
        case 4: return Theme.safe
        default: return Theme.textDim
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 14
        height: 14
        radius: 7
        color: root.dotColor
        opacity: 0.22
        visible: root.linkState === 2 || root.linkState === 1
    }

    Rectangle {
        anchors.centerIn: parent
        width: 8
        height: 8
        radius: 4
        color: root.dotColor
        opacity: root.linkState === 1 ? root.pulse : 1
        Behavior on color { ColorAnimation { duration: 180 } }
    }

    SequentialAnimation on pulse {
        running: root.linkState === 1
        loops: 3
        NumberAnimation { from: 0.25; to: 1; duration: 300; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0.25; duration: 300; easing.type: Easing.InOutSine }
    }
}
