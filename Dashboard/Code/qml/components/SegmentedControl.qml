import QtQuick

Rectangle {
    id: root
    property var model: ["24H", "7D", "30D"]
    property int currentIndex: 0
    signal indexChanged(int index)

    color: Theme.bgInset
    border.color: Theme.border
    border.width: 1
    radius: 5
    height: 28
    width: row.implicitWidth + 6

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 2
        Repeater {
            model: root.model
            delegate: Item {
                width: Math.max(44, label.implicitWidth + 16)
                height: 22
                Rectangle {
                    anchors.fill: parent
                    radius: 3
                    color: root.currentIndex === index ? Theme.bgCard : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData
                    color: root.currentIndex === index ? Theme.text : Theme.textMuted
                    font.family: App.uiFont
                    font.pixelSize: 11
                    font.weight: root.currentIndex === index ? Font.DemiBold : Font.Normal
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        root.currentIndex = index
                        root.indexChanged(index)
                    }
                }
            }
        }
    }
}
