import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: Theme.bgDeep

    property real formOpacity: 0
    property real formLift: 12

    function submit() {
        const role = String(userField.text).trim().toLowerCase()
        if (role === "guest") {
            Auth.loginAsGuest()
            return
        }
        if (role === "admin") {
            Auth.login("admin", passField.text)
            return
        }
        Auth.loginAsRole(userField.text)
    }

    Component.onCompleted: {
        mount.start()
        userField.forceActiveFocus()
    }

    ParallelAnimation {
        id: mount
        NumberAnimation { target: root; property: "formOpacity"; to: 1; duration: Theme.motion; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "formLift"; to: 0; duration: Theme.motion; easing.type: Easing.OutCubic }
    }

    Connections {
        target: Auth
        function onLoginRejected() { shake.restart() }
    }

    Canvas {
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = Theme.bgDeep
            ctx.fillRect(0, 0, width, height)
            ctx.fillStyle = "#121A18"
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(width * 0.58, 0)
            ctx.lineTo(0, height * 0.78)
            ctx.closePath()
            ctx.fill()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Column {
        id: form
        width: 400
        spacing: 22
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.formLift
        opacity: root.formOpacity

        SequentialAnimation {
            id: shake
            NumberAnimation { target: form; property: "anchors.horizontalCenterOffset"; to: 7; duration: 45 }
            NumberAnimation { target: form; property: "anchors.horizontalCenterOffset"; to: -7; duration: 45 }
            NumberAnimation { target: form; property: "anchors.horizontalCenterOffset"; to: 4; duration: 45 }
            NumberAnimation { target: form; property: "anchors.horizontalCenterOffset"; to: 0; duration: 45 }
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: "USER LOGIN"
            color: Theme.text
            font.family: App.uiFont
            font.pixelSize: 28
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
        }

        Item { width: 1; height: 10 }

        Item {
            width: parent.width
            height: 56

            Rectangle {
                id: userPill
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 52
                radius: 26
                color: Qt.rgba(1, 1, 1, userField.activeFocus ? 0.14 : 0.10)
                border.width: 1
                border.color: userField.activeFocus
                              ? Qt.rgba(Theme.safe.r, Theme.safe.g, Theme.safe.b, 0.45)
                              : Qt.rgba(1, 1, 1, 0.08)
                Behavior on color { ColorAnimation { duration: Theme.motion } }
                Behavior on border.color { ColorAnimation { duration: Theme.motion } }
            }

            TextField {
                id: userField
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 64
                anchors.rightMargin: 22
                height: 52
                verticalAlignment: Text.AlignVCenter
                color: Theme.text
                font.family: App.uiFont
                font.pixelSize: 15
                selectByMouse: true
                placeholderText: "admin or guest"
                placeholderTextColor: Theme.textDim
                background: Item {}
                onAccepted: passField.forceActiveFocus()
            }

            Rectangle {
                width: 50
                height: 50
                radius: 25
                x: 3
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.text

                Canvas {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = Theme.bgDeep
                        ctx.lineWidth = 1.6
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        ctx.beginPath()
                        ctx.arc(width * 0.5, height * 0.32, width * 0.20, 0, Math.PI * 2)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(width * 0.5, height * 1.08, width * 0.38, Math.PI * 1.15, Math.PI * 1.85)
                        ctx.stroke()
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: 56

            Rectangle {
                id: passPill
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 52
                radius: 26
                color: Qt.rgba(1, 1, 1, passField.activeFocus ? 0.14 : 0.10)
                border.width: 1
                border.color: passField.activeFocus
                              ? Qt.rgba(Theme.safe.r, Theme.safe.g, Theme.safe.b, 0.45)
                              : Qt.rgba(1, 1, 1, 0.08)
                Behavior on color { ColorAnimation { duration: Theme.motion } }
                Behavior on border.color { ColorAnimation { duration: Theme.motion } }
            }

            TextField {
                id: passField
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                anchors.rightMargin: 64
                height: 52
                verticalAlignment: Text.AlignVCenter
                color: Theme.text
                font.family: App.uiFont
                font.pixelSize: 15
                echoMode: TextInput.Password
                selectByMouse: true
                placeholderText: "Password"
                placeholderTextColor: Theme.textDim
                background: Item {}
                onAccepted: root.submit()
            }

            Rectangle {
                width: 50
                height: 50
                radius: 25
                anchors.right: parent.right
                anchors.rightMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.text

                Canvas {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = Theme.bgDeep
                        ctx.lineWidth = 1.6
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        const x = 5
                        const y = 11
                        const bw = 12
                        const bh = 8
                        ctx.beginPath()
                        ctx.moveTo(x + 2, y)
                        ctx.lineTo(x + 2, y - 4.2)
                        ctx.arc(x + bw * 0.5, y - 4.2, bw * 0.5 - 2, Math.PI, 0, false)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(x, y)
                        ctx.lineTo(x + bw, y)
                        ctx.lineTo(x + bw, y + bh)
                        ctx.lineTo(x, y + bh)
                        ctx.closePath()
                        ctx.stroke()
                    }
                }
            }
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: Auth.errorMessage
            color: Theme.critical
            font.family: App.uiFont
            font.pixelSize: 12
            opacity: Auth.errorMessage.length ? 1 : 0
            height: Auth.errorMessage.length ? implicitHeight : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion } }
            Behavior on height { NumberAnimation { duration: Theme.motion } }
        }

        Rectangle {
            width: parent.width
            height: 52
            radius: 26
            color: enterMouse.containsMouse ? Qt.lighter(Theme.text, 1.04) : Theme.text
            opacity: Auth.locked ? 0.4 : 1
            Behavior on color { ColorAnimation { duration: Theme.motion } }

            Text {
                anchors.centerIn: parent
                text: Auth.locked ? ("LOCKED  ·  " + Auth.lockRemaining + " S") : "LOGIN"
                color: Theme.bgDeep
                font.family: App.uiFont
                font.pixelSize: 16
                font.weight: Font.DemiBold
                font.letterSpacing: 1.6
            }

            MouseArea {
                id: enterMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: !Auth.locked
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.submit()
            }
        }
    }
}
