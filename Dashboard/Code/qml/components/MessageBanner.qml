import QtQuick

Rectangle {
    id: root

    property string text: ""
    property string kind: "warning"
    property int eventType: -1
    property bool compact: false
    property bool fillWidth: false

    readonly property color fill: eventType >= 0 ? Theme.eventFill(eventType) : Theme.kindFill(kind)

    implicitWidth: fillWidth ? (parent ? parent.width : label.implicitWidth + hPad * 2)
                             : label.implicitWidth + hPad * 2
    implicitHeight: Math.max(compact ? 20 : 30, label.implicitHeight + vPad * 2)
    width: fillWidth && parent ? parent.width : implicitWidth
    radius: Theme.radiusCtl
    color: fill
    visible: text.length > 0
    clip: true

    readonly property int hPad: compact ? 8 : 12
    readonly property int vPad: compact ? 3 : 7

    Text {
        id: label
        anchors.left: parent.left
        anchors.right: fillWidth ? parent.right : undefined
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: root.hPad
        anchors.rightMargin: fillWidth ? root.hPad : 0
        width: fillWidth ? parent.width - root.hPad * 2 : implicitWidth
        text: root.text
        color: "#F7F4EF"
        wrapMode: fillWidth ? Text.WordWrap : Text.NoWrap
        elide: fillWidth ? Text.ElideNone : Text.ElideRight
        font.family: App.uiFont
        font.pixelSize: compact ? 10 : 12
        font.weight: Font.DemiBold
    }
}
