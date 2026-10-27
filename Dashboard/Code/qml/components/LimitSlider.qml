import QtQuick
import QtQuick.Controls

Slider {
    id: root
    property color fillColor: Theme.warning
    implicitHeight: 28
    leftPadding: 0
    rightPadding: 0

    background: Item {
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - 4) / 2
        width: root.availableWidth
        height: 4
        Rectangle {
            anchors.fill: parent
            radius: 2
            color: Theme.bgInset
            border.color: Theme.border
            border.width: 1
        }
        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: root.fillColor
        }
    }

    handle: Item {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + (root.availableHeight - height) / 2
        width: 18
        height: 18
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            width: 18
            height: 18
            radius: 9
            color: Qt.rgba(0, 0, 0, 0.28)
        }
        Rectangle {
            anchors.centerIn: parent
            width: 16
            height: 16
            radius: 8
            color: Theme.text
            border.color: root.fillColor
            border.width: 2
        }
    }
}
