import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Item {
    id: root

    Rectangle {
        id: filterBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.padPage
        height: 48
        color: Theme.bgCard
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusCard

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 16
            spacing: 10

            ComboBox {
                id: typeBox
                width: 190
                height: 32
                model: ["All events", "Warning", "Critical", "Emergency-Interrupt"]
                font.family: App.uiFont
                font.pixelSize: 13
                onActivated: App.alerts.typeFilter = index === 0 ? -1 : index - 1
                background: Rectangle { color: Theme.bgInset; border.color: Theme.border; radius: Theme.radiusCtl }
                contentItem: Text {
                    text: typeBox.displayText
                    color: Theme.text
                    font: typeBox.font
                    leftPadding: 10
                    verticalAlignment: Text.AlignVCenter
                }
            }
            ComboBox {
                id: zoneBox
                width: 240
                height: 32
                model: App.zoneNames
                font.family: App.uiFont
                font.pixelSize: 13
                displayText: currentIndex <= 0 ? "All zones" : currentText
                onActivated: App.alerts.zoneFilter = index <= 0 ? "" : currentText
                background: Rectangle { color: Theme.bgInset; border.color: Theme.border; radius: Theme.radiusCtl }
                contentItem: Text {
                    text: zoneBox.displayText
                    color: Theme.text
                    font: zoneBox.font
                    leftPadding: 10
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            ComboBox {
                id: dateBox
                width: 140
                height: 32
                model: ["Any time", "Last 24h", "Last 7d"]
                font.family: App.uiFont
                font.pixelSize: 13
                onActivated: App.alerts.dateFilter = index
                background: Rectangle { color: Theme.bgInset; border.color: Theme.border; radius: Theme.radiusCtl }
                contentItem: Text {
                    text: dateBox.displayText
                    color: Theme.text
                    font: dateBox.font
                    leftPadding: 10
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 16
            spacing: 8
            IndustrialButton {
                destructive: true
                enabled: Auth.isAdmin
                text: "Clear log"
                onClicked: App.clearAlerts()
            }
            IndustrialButton {
                primary: true
                text: "Export CSV"
                onClicked: saveDialog.open()
            }
        }
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV files (*.csv)"]
        defaultSuffix: "csv"
        onAccepted: App.exportAlerts(selectedFile)
    }

    Rectangle {
        anchors.top: filterBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.padPage
        anchors.topMargin: 12
        color: Theme.bgCard
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusCard
        clip: true

        Rectangle {
            id: header
            width: parent.width
            height: 44
            color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.55)
            z: 2
            Row {
                anchors.fill: parent
                anchors.leftMargin: 16
                spacing: 0
                Repeater {
                    model: [
                        { t: "Timestamp", w: 190 },
                        { t: "Device", w: 90 },
                        { t: "Zone", w: 240 },
                        { t: "Event", w: 170 },
                        { t: "Trigger", w: 180 },
                        { t: "", w: 48 }
                    ]
                    delegate: Text {
                        width: modelData.w
                        height: 44
                        text: modelData.t
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        ListView {
            id: list
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            model: App.alerts
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: row
                width: list.width
                height: 48
                color: {
                    if (eventType === 0)
                        return Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.08)
                    if (eventType >= 1)
                        return Qt.rgba(Theme.critical.r, Theme.critical.g, Theme.critical.b, 0.08)
                    return index % 2 === 0 ? Theme.bgCard : Theme.bgInset
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    spacing: 0
                    Text {
                        width: 190; height: parent.height
                        text: timestampText
                        color: Theme.text
                        font.family: App.monoFont
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: 90; height: parent.height
                        text: idString
                        color: Theme.text
                        font.family: App.monoFont
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: 240; height: parent.height
                        text: zone
                        elide: Text.ElideRight
                        color: Theme.textMuted
                        font.family: App.uiFont
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: 170; height: parent.height
                        text: typeLabel
                        color: Theme.eventFill(eventType)
                        font.family: App.uiFont
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: 180; height: parent.height
                        text: triggerValue
                        color: Theme.text
                        font.family: App.monoFont
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                    Item {
                        width: 48; height: parent.height
                        Rectangle {
                            anchors.centerIn: parent
                            width: 26
                            height: 26
                            radius: 13
                            color: acknowledged ? Qt.rgba(Theme.safe.r, Theme.safe.g, Theme.safe.b, 0.16) : "transparent"
                            border.color: acknowledged ? Theme.safe : Theme.border
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: acknowledged ? "✓" : "ACK"
                                color: acknowledged ? Theme.safe : Theme.textDim
                                font.family: App.uiFont
                                font.pixelSize: acknowledged ? 12 : 8
                                font.weight: Font.DemiBold
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: Auth.isAdmin && !acknowledged
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.acknowledgeAlert(alertId)
                            }
                        }
                    }
                }
            }
        }
    }
}
