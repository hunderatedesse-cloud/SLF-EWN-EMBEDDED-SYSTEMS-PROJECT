import QtQuick
import QtQuick.Controls

Item {
    id: root
    property bool expanded: true
    property bool popupOpen: false

    height: expanded ? 248 : 64

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.borderSoft
        opacity: root.expanded ? 1 : 0
    }

    Column {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusRow.top
        anchors.bottomMargin: 8
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8
        visible: root.expanded
        opacity: root.expanded ? 1 : 0

        ComboBox {
            id: portBox
            enabled: Auth.isAdmin && App.connectionState !== 2
            width: parent.width
            height: 28
            model: App.availablePorts
            font.family: App.monoFont
            font.pixelSize: 11
            onActivated: function(index) { App.portName = App.availablePorts[index] }

            function selectCurrent() {
                const ports = App.availablePorts
                for (let i = 0; i < ports.length; ++i) {
                    if (ports[i] === App.portName) {
                        if (currentIndex !== i)
                            currentIndex = i
                        return
                    }
                }
            }
            Component.onCompleted: selectCurrent()

            Connections {
                target: App
                function onPortsChanged() { portBox.selectCurrent() }
                function onConnectionChanged() { portBox.selectCurrent() }
            }

            background: Rectangle {
                implicitHeight: 28
                color: Theme.bgInset
                border.color: portBox.popup.visible ? Theme.action : Theme.border
                border.width: 1
                radius: 4
            }
            contentItem: Text {
                leftPadding: 8
                rightPadding: 20
                elide: Text.ElideRight
                text: {
                    if (!App.availablePorts.length)
                        return "No ports found"
                    const i = portBox.currentIndex
                    if (i >= 0 && i < App.availablePortLabels.length)
                        return App.availablePortLabels[i]
                    return portBox.displayText
                }
                color: App.availablePorts.length ? Theme.text : Theme.textDim
                font: portBox.font
                verticalAlignment: Text.AlignVCenter
            }
            popup: Popup {
                y: portBox.height + 2
                width: portBox.width
                implicitHeight: Math.min(contentItem.implicitHeight + 2, 180)
                padding: 1
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                onVisibleChanged: root.popupOpen = visible
                background: Rectangle {
                    color: Theme.bgCard
                    border.color: Theme.border
                    border.width: 1
                    radius: 4
                }
                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: portBox.popup.visible ? portBox.delegateModel : null
                    currentIndex: portBox.highlightedIndex
                    ScrollIndicator.vertical: ScrollIndicator {}
                }
            }
            delegate: ItemDelegate {
                width: portBox.width
                height: 26
                highlighted: portBox.highlightedIndex === index
                background: Rectangle {
                    color: highlighted ? Theme.bgCard : "transparent"
                }
                contentItem: Text {
                    leftPadding: 8
                    text: (index >= 0 && index < App.availablePortLabels.length)
                          ? App.availablePortLabels[index] : modelData
                    color: Theme.text
                    font.family: App.monoFont
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Row {
            width: parent.width
            spacing: 8

            Text {
                text: "115200  8N1"
                color: Theme.textDim
                font.family: App.monoFont
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "· rescan"
                color: rescanMouse.containsMouse ? Theme.action : Theme.textDim
                font.family: App.uiFont
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
                MouseArea {
                    id: rescanMouse
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.refreshPorts()
                }
            }
        }

        IndustrialButton {
            enabled: Auth.isAdmin
            width: parent.width
            compact: true
            primary: App.connectionState !== 2
            destructive: App.connectionState === 2
            text: App.connectionState === 2 ? "Disconnect" : (App.connectionState === 1 ? "Connecting…" : "Connect")
            onClicked: {
                if (App.connectionState === 2)
                    App.disconnectSerial()
                else {
                    App.refreshPorts()
                    App.connectSerial()
                }
            }
        }
        IndustrialButton {
            enabled: App.connectionState === 2 && !App.dumpBusy
            width: parent.width
            compact: true
            primary: true
            text: App.dumpBusy ? (App.dumpPhase.length ? App.dumpPhase : "Working…") : "Refresh"
            onClicked: App.refreshFromReceiver()
        }
    }

    Row {
        id: statusRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.expanded ? 12 : 0
        anchors.rightMargin: root.expanded ? 12 : 0
        anchors.bottomMargin: 12
        spacing: 8
        height: 20

        Item {
            width: root.expanded ? implicitWidth : parent.width
            height: parent.height
            implicitWidth: inner.implicitWidth

            Row {
                id: inner
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: root.expanded ? undefined : parent.horizontalCenter

                StatusDot {
                    linkState: App.connectionState
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    visible: root.expanded
                    text: App.dumpBusy && App.dumpPhase.length ? App.dumpPhase
                        : (App.connectionState === 4 ? "SIMULATED" : "SERIAL")
                    color: App.dumpBusy ? Theme.action
                        : (App.connectionState === 4 ? Theme.warning : Theme.textDim)
                    font.family: App.uiFont
                    font.pixelSize: 10
                    font.letterSpacing: 1.1
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
