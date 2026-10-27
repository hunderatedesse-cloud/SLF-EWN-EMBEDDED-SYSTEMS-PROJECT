import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property real probeM: 30
    property real probeT: 4
    property var userRows: []

    function numOrNaN(s) {
        const t = String(s).trim()
        if (!t.length)
            return Number.NaN
        const v = Number(t)
        return isFinite(v) ? v : Number.NaN
    }
    function coordText(v) {
        return (typeof v === "number" && isFinite(v)) ? Number(v).toFixed(6) : ""
    }
    function altText(v) {
        return (typeof v === "number" && isFinite(v)) ? Number(v).toFixed(1) : ""
    }

    readonly property bool admin: Auth.isAdmin

    function reloadUsers() {
        userRows = Auth.userList()
    }

    Component.onCompleted: reloadUsers()

    Connections {
        target: Auth
        function onUsersChanged() { root.reloadUsers() }
        function onSessionChanged() { root.reloadUsers() }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        color: Theme.bgCard
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusCard

        Rectangle {
            id: tabBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 12
            anchors.leftMargin: 16
            height: 36
            radius: 18
            color: Theme.bgInset
            border.color: Theme.border
            border.width: 1
            width: tabRow.implicitWidth + 6

            Row {
                id: tabRow
                anchors.centerIn: parent
                spacing: 2
                property int currentIndex: 0

                Repeater {
                    model: ["Thresholds", "Places", "Nodes", "Link", "Access"]
                    delegate: Rectangle {
                        width: tabLabel.implicitWidth + 22
                        height: 30
                        radius: 15
                        color: tabRow.currentIndex === index ? Theme.bgElevated : "transparent"
                        border.color: tabRow.currentIndex === index
                                      ? Qt.rgba(Theme.safe.r, Theme.safe.g, Theme.safe.b, 0.35) : "transparent"
                        border.width: 1
                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: tabRow.currentIndex === index ? Theme.text : Theme.textDim
                            font.family: App.uiFont
                            font.pixelSize: 12
                            font.weight: tabRow.currentIndex === index ? Font.DemiBold : Font.Medium
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tabRow.currentIndex = index
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.top: tabBar.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }

        Rectangle {
            id: readOnlyBanner
            visible: !root.admin
            anchors.top: tabBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            anchors.topMargin: 10
            height: 36
            radius: 10
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.16)
            border.color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.40)
            border.width: 1
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "Read-only session — configuration is locked for guest operators."
                color: Theme.warning
                font.family: App.uiFont
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }

        StackLayout {
            id: stack
            anchors.top: readOnlyBanner.visible ? readOnlyBanner.bottom : tabBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 24
            currentIndex: tabRow.currentIndex

            // ---------------- Thresholds ----------------
            Flickable {
                clip: true
                contentHeight: threshRow.height + 8
                contentWidth: width

                Row {
                    id: threshRow
                    width: stack.width
                    spacing: 24
                    enabled: root.admin
                    opacity: root.admin ? 1 : 0.55

                    Column {
                        id: threshCol
                        width: Math.max(320, Math.min(520, parent.width - 348))
                        spacing: 20

                        Text {
                            text: "ALARM LIMITS"
                            color: Theme.textDim
                            font.family: App.uiFont
                            font.pixelSize: 11
                            font.letterSpacing: 1.4
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: "Default alarm limits for every node that does not override them under Nodes."
                            color: Theme.textMuted
                            font.family: App.uiFont
                            font.pixelSize: 12
                            width: parent.width
                            wrapMode: Text.WordWrap
                        }

                        Row {
                            width: parent.width
                            spacing: 28

                            Column {
                                spacing: 16
                                width: (parent.width - 28) / 2
                                Text {
                                    text: "Moisture"
                                    color: Theme.text
                                    font.family: App.uiFont
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                                Column {
                                    spacing: 6
                                    width: parent.width
                                    Text { text: "Warning  %"; color: Theme.textMuted; font.pixelSize: 11; font.weight: Font.Medium; font.family: App.uiFont }
                                    Row {
                                        spacing: 10
                                        LimitSlider {
                                            id: mw
                                            width: Math.max(120, parent.parent.width - 62)
                                            from: 5; to: 90
                                            value: App.moistureWarning
                                            fillColor: Theme.warning
                                            onMoved: App.moistureWarning = value
                                        }
                                        Text { text: mw.value.toFixed(1); color: Theme.text; font.family: App.monoFont; font.pixelSize: 13; width: 40; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                }
                                Column {
                                    spacing: 6
                                    width: parent.width
                                    Text { text: "Critical  %"; color: Theme.textMuted; font.pixelSize: 11; font.weight: Font.Medium; font.family: App.uiFont }
                                    Row {
                                        spacing: 10
                                        LimitSlider {
                                            id: mc
                                            width: Math.max(120, parent.parent.width - 62)
                                            from: 10; to: 95
                                            value: App.moistureCritical
                                            fillColor: Theme.critical
                                            onMoved: App.moistureCritical = value
                                        }
                                        Text { text: mc.value.toFixed(1); color: Theme.text; font.family: App.monoFont; font.pixelSize: 13; width: 40; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                }
                            }

                            Column {
                                spacing: 16
                                width: (parent.width - 28) / 2
                                Text {
                                    text: "Tilt"
                                    color: Theme.text
                                    font.family: App.uiFont
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                                Column {
                                    spacing: 6
                                    width: parent.width
                                    Text { text: "Warning  °"; color: Theme.textMuted; font.pixelSize: 11; font.weight: Font.Medium; font.family: App.uiFont }
                                    Row {
                                        spacing: 10
                                        LimitSlider {
                                            id: tw
                                            width: Math.max(120, parent.parent.width - 62)
                                            from: 1; to: 180
                                            value: App.tiltWarning
                                            fillColor: Theme.warning
                                            onMoved: App.tiltWarning = value
                                        }
                                        Text { text: tw.value.toFixed(1); color: Theme.text; font.family: App.monoFont; font.pixelSize: 13; width: 40; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                }
                                Column {
                                    spacing: 6
                                    width: parent.width
                                    Text { text: "Critical  °"; color: Theme.textMuted; font.pixelSize: 11; font.weight: Font.Medium; font.family: App.uiFont }
                                    Row {
                                        spacing: 10
                                        LimitSlider {
                                            id: tc
                                            width: Math.max(120, parent.parent.width - 62)
                                            from: 2; to: 180
                                            value: App.tiltCritical
                                            fillColor: Theme.critical
                                            onMoved: App.tiltCritical = value
                                        }
                                        Text { text: tc.value.toFixed(1); color: Theme.text; font.family: App.monoFont; font.pixelSize: 13; width: 40; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: previewCard
                        width: 300
                        height: 412
                        radius: Theme.radiusCard
                        color: Theme.bgElevated
                        border.color: Theme.border
                        border.width: 1

                        readonly property int probeStatus: App.probeStatus(root.probeM, root.probeT)
                        readonly property color previewTone: Theme.statusColor(probeStatus)

                        Column {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 0

                            Text {
                                text: "LIVE PREVIEW"
                                color: Theme.textDim
                                font.family: App.uiFont
                                font.pixelSize: 11
                                font.letterSpacing: 1.4
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: "Drag the sliders — status updates against the limits on the left."
                                color: Theme.textMuted
                                font.family: App.uiFont
                                font.pixelSize: 11
                                width: parent.width
                                wrapMode: Text.WordWrap
                            }

                            Item { width: 1; height: 16 }

                            StatusChip {
                                text: Theme.statusLabel(previewCard.probeStatus)
                                tone: previewCard.previewTone
                            }

                            Item { width: 1; height: 14 }

                            Text {
                                text: root.probeM.toFixed(1) + "%"
                                color: Theme.text
                                font.family: App.monoFont
                                font.pixelSize: 30
                                font.weight: Font.Bold
                            }
                            Text {
                                text: "Moisture"
                                color: Theme.textMuted
                                font.family: App.uiFont
                                font.pixelSize: 11
                                font.weight: Font.Medium
                            }

                            Item { width: 1; height: 8 }

                            Text {
                                text: root.probeT.toFixed(1) + "°"
                                color: Theme.text
                                font.family: App.monoFont
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: "Tilt magnitude"
                                color: Theme.textMuted
                                font.family: App.uiFont
                                font.pixelSize: 11
                                font.weight: Font.Medium
                            }

                            Item { width: 1; height: 16 }

                            Text { text: "Probe moisture"; color: Theme.textMuted; font.pixelSize: 11; font.weight: Font.Medium; font.family: App.uiFont }
                            LimitSlider {
                                width: parent.width
                                from: 0; to: 100
                                value: root.probeM
                                fillColor: previewCard.previewTone
                                onMoved: root.probeM = value
                            }

                            Item { width: 1; height: 10 }

                            Text { text: "Probe tilt"; color: Theme.textMuted; font.pixelSize: 11; font.weight: Font.Medium; font.family: App.uiFont }
                            LimitSlider {
                                width: parent.width
                                from: 0; to: 40
                                value: root.probeT
                                fillColor: Theme.warning
                                onMoved: root.probeT = value
                            }
                        }
                    }
                }
            }

            // ---------------- Places ----------------
            Flickable {
                clip: true
                contentHeight: placeCol.height
                Column {
                    id: placeCol
                    width: Math.min(680, stack.width)
                    spacing: 16

                    Text {
                        text: "SITES / PLACES"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: "Create a named site with a start and end geographic point. Nodes whose latitude, longitude and altitude fall inside that box are assigned automatically."
                        color: Theme.textMuted
                        font.family: App.uiFont
                        font.pixelSize: 12
                    }

                    Repeater {
                        model: App.zones
                        delegate: Rectangle {
                            width: placeCol.width
                            height: 72
                            radius: 8
                            color: Theme.bgInset
                            border.color: Theme.border
                            border.width: 1
                            Row {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: modelData.color
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Column {
                                    spacing: 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 180
                                    Text {
                                        text: modelData.name
                                        color: Theme.text
                                        font.family: App.uiFont
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        width: parent.width
                                        wrapMode: Text.WordWrap
                                        text: modelData.coverage
                                        color: modelData.hasCoverage ? Theme.textMuted : Theme.textDim
                                        font.family: App.uiFont
                                        font.pixelSize: 11
                                    }
                                }
                                Item { width: 1; height: 1 }
                            }
                            Row {
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8
                                IndustrialButton {
                                    compact: true
                                    text: "Edit"
                                    enabled: root.admin
                                    onClicked: {
                                        placeId.text = modelData.id
                                        placeName.text = modelData.name
                                        placeNotes.text = modelData.notes
                                        px.value = modelData.x
                                        py.value = modelData.y
                                        pw.value = modelData.w
                                        ph.value = modelData.h
                                        pLat1.text = root.coordText(modelData.lat1)
                                        pLon1.text = root.coordText(modelData.lon1)
                                        pAlt1.text = root.altText(modelData.alt1)
                                        pLat2.text = root.coordText(modelData.lat2)
                                        pLon2.text = root.coordText(modelData.lon2)
                                        pAlt2.text = root.altText(modelData.alt2)
                                    }
                                }
                                IndustrialButton {
                                    compact: true
                                    destructive: true
                                    text: "Delete"
                                    enabled: root.admin
                                    onClicked: App.deletePlace(modelData.id)
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }

                    Text {
                        text: placeId.text.length ? "EDIT PLACE" : "NEW PLACE"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    FieldInput { id: placeName; width: parent.width; label: "NAME"; placeholder: "e.g. North Slope" }
                    FieldInput { id: placeNotes; width: parent.width; label: "NOTES"; placeholder: "optional description" }
                    Text { id: placeId; visible: false; text: "" }

                    Text {
                        text: "COVERAGE  ·  START POINT"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }
                    Row {
                        spacing: 12
                        FieldInput { id: pLat1; width: 210; label: "LATITUDE"; placeholder: "e.g. 31.9522" }
                        FieldInput { id: pLon1; width: 210; label: "LONGITUDE"; placeholder: "e.g. 35.9106" }
                        FieldInput { id: pAlt1; width: 210; label: "ALTITUDE m"; placeholder: "e.g. 820" }
                    }
                    Text {
                        text: "COVERAGE  ·  END POINT"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }
                    Row {
                        spacing: 12
                        FieldInput { id: pLat2; width: 210; label: "LATITUDE"; placeholder: "e.g. 31.9800" }
                        FieldInput { id: pLon2; width: 210; label: "LONGITUDE"; placeholder: "e.g. 35.9400" }
                        FieldInput { id: pAlt2; width: 210; label: "ALTITUDE m"; placeholder: "e.g. 910" }
                    }

                    Grid {
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 10
                        width: parent.width
                        Column {
                            spacing: 4
                            Text { text: "Map X  " + px.value.toFixed(2); color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                            Slider { id: px; width: 260; from: 0; to: 0.85; value: 0.08 }
                        }
                        Column {
                            spacing: 4
                            Text { text: "Map Y  " + py.value.toFixed(2); color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                            Slider { id: py; width: 260; from: 0; to: 0.85; value: 0.08 }
                        }
                        Column {
                            spacing: 4
                            Text { text: "Width  " + pw.value.toFixed(2); color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                            Slider { id: pw; width: 260; from: 0.08; to: 0.6; value: 0.28 }
                        }
                        Column {
                            spacing: 4
                            Text { text: "Height  " + ph.value.toFixed(2); color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                            Slider { id: ph; width: 260; from: 0.08; to: 0.6; value: 0.24 }
                        }
                    }

                    Row {
                        spacing: 10
                        IndustrialButton {
                            primary: true
                            enabled: root.admin
                            text: placeId.text.length ? "Save place" : "Create place"
                            onClicked: {
                                if (placeName.text.length < 2)
                                    return
                                const lat1 = root.numOrNaN(pLat1.text)
                                const lon1 = root.numOrNaN(pLon1.text)
                                const alt1 = root.numOrNaN(pAlt1.text)
                                const lat2 = root.numOrNaN(pLat2.text)
                                const lon2 = root.numOrNaN(pLon2.text)
                                const alt2 = root.numOrNaN(pAlt2.text)
                                if (placeId.text.length)
                                    App.updatePlace(placeId.text, placeName.text, placeNotes.text, px.value, py.value, pw.value, ph.value, lat1, lon1, alt1, lat2, lon2, alt2)
                                else
                                    App.createPlace(placeName.text, placeNotes.text, px.value, py.value, pw.value, ph.value, lat1, lon1, alt1, lat2, lon2, alt2)
                                placeId.text = ""
                                placeName.text = ""
                                placeNotes.text = ""
                                pLat1.text = ""; pLon1.text = ""; pAlt1.text = ""
                                pLat2.text = ""; pLon2.text = ""; pAlt2.text = ""
                            }
                        }
                        IndustrialButton {
                            visible: placeId.text.length > 0
                            text: "Cancel edit"
                            onClicked: {
                                placeId.text = ""
                                placeName.text = ""
                                placeNotes.text = ""
                                pLat1.text = ""; pLon1.text = ""; pAlt1.text = ""
                                pLat2.text = ""; pLon2.text = ""; pAlt2.text = ""
                            }
                        }
                    }
                }
            }

            // ---------------- Nodes ----------------
            Flickable {
                clip: true
                contentHeight: nodeCol.height
                Column {
                    id: nodeCol
                    width: Math.min(760, stack.width)
                    spacing: 12

                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: App.mapNodes.length
                              ? "Set each node’s latitude, longitude and altitude. If it sits inside a place’s start–end box it is assigned automatically. Alarm limits can be overridden per node."
                              : "No nodes yet — connect the receiver. NODE260 (and others) show up here on first packet."
                        color: Theme.textMuted
                        font.family: App.uiFont
                        font.pixelSize: 12
                    }

                    Repeater {
                        model: App.mapNodes
                        delegate: Rectangle {
                            width: nodeCol.width
                            height: customTh.checked ? 268 : 168
                            radius: 8
                            color: Theme.bgInset
                            border.color: modelData.assigned ? Theme.border : Theme.warning

                            Column {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 10

                                Row {
                                    spacing: 12
                                    width: parent.width
                                    Rectangle {
                                        width: 10; height: 10; radius: 5
                                        color: Theme.statusColor(modelData.status)
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        width: 88
                                        text: modelData.idString
                                        color: Theme.text
                                        font.family: App.monoFont
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    ComboBox {
                                        id: nodePlace
                                        width: 220
                                        height: 28
                                        enabled: root.admin
                                        font.family: App.uiFont
                                        font.pixelSize: 12
                                        model: {
                                            const rows = [{ id: "none", name: "Unassigned" }]
                                            const z = App.zones
                                            for (let i = 0; i < z.length; ++i)
                                                rows.push(z[i])
                                            return rows
                                        }
                                        textRole: "name"
                                        displayText: currentIndex >= 0 && model && model[currentIndex] ? model[currentIndex].name : "Unassigned"
                                        delegate: ItemDelegate {
                                            width: nodePlace.width
                                            highlighted: nodePlace.highlightedIndex === index
                                            contentItem: Text {
                                                text: modelData.name
                                                color: Theme.text
                                                font.family: App.uiFont
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                            background: Rectangle { color: highlighted ? Theme.bgCardHover : Theme.bgCard }
                                        }
                                        Component.onCompleted: {
                                            const zid = modelData.zoneId
                                            for (let i = 0; i < count; ++i) {
                                                if (model[i].id === zid) { currentIndex = i; return }
                                            }
                                            currentIndex = 0
                                        }
                                        onActivated: function(index) {
                                            const row = model[index]
                                            App.assignNode(modelData.deviceId, row ? row.id : "none")
                                        }
                                        background: Rectangle {
                                            implicitHeight: 28
                                            color: Theme.bgCard
                                            border.color: Theme.border
                                            radius: 4
                                        }
                                        contentItem: Text {
                                            leftPadding: 8
                                            text: nodePlace.displayText
                                            color: Theme.text
                                            font: nodePlace.font
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Text {
                                        text: modelData.autoAssigned ? "auto-assigned" : (modelData.everSeen ? "live" : "configured")
                                        color: modelData.autoAssigned ? Theme.action : (modelData.everSeen ? Theme.safe : Theme.textDim)
                                        font.family: App.uiFont
                                        font.pixelSize: 11
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Item { width: Math.max(8, parent.width - 620); height: 1 }
                                    IndustrialButton {
                                        compact: true
                                        destructive: true
                                        enabled: root.admin
                                        text: "Delete"
                                        onClicked: App.deleteNode(modelData.deviceId)
                                    }
                                }

                                Row {
                                    spacing: 12
                                    Column {
                                        spacing: 4
                                        Text { text: "LATITUDE"; color: Theme.textDim; font.pixelSize: 10; font.family: App.uiFont; font.weight: Font.DemiBold }
                                        TextField {
                                            id: nLat
                                            width: 150; height: 28
                                            enabled: root.admin
                                            text: root.coordText(modelData.latitude)
                                            color: Theme.text
                                            font.family: App.monoFont
                                            font.pixelSize: 12
                                            background: Rectangle { color: Theme.bgCard; border.color: Theme.border; radius: 4 }
                                        }
                                    }
                                    Column {
                                        spacing: 4
                                        Text { text: "LONGITUDE"; color: Theme.textDim; font.pixelSize: 10; font.family: App.uiFont; font.weight: Font.DemiBold }
                                        TextField {
                                            id: nLon
                                            width: 150; height: 28
                                            enabled: root.admin
                                            text: root.coordText(modelData.longitude)
                                            color: Theme.text
                                            font.family: App.monoFont
                                            font.pixelSize: 12
                                            background: Rectangle { color: Theme.bgCard; border.color: Theme.border; radius: 4 }
                                        }
                                    }
                                    Column {
                                        spacing: 4
                                        Text { text: "ALTITUDE m"; color: Theme.textDim; font.pixelSize: 10; font.family: App.uiFont; font.weight: Font.DemiBold }
                                        TextField {
                                            id: nAlt
                                            width: 120; height: 28
                                            enabled: root.admin
                                            text: root.altText(modelData.altitude)
                                            color: Theme.text
                                            font.family: App.monoFont
                                            font.pixelSize: 12
                                            background: Rectangle { color: Theme.bgCard; border.color: Theme.border; radius: 4 }
                                        }
                                    }
                                    IndustrialButton {
                                        anchors.verticalCenter: parent.verticalCenter
                                        primary: true
                                        compact: true
                                        enabled: root.admin
                                        text: "Save location"
                                        onClicked: App.setNodeGeo(modelData.deviceId, root.numOrNaN(nLat.text), root.numOrNaN(nLon.text), root.numOrNaN(nAlt.text))
                                    }
                                }

                                CheckBox {
                                    id: customTh
                                    enabled: root.admin
                                    text: "Custom alarm limits for this node"
                                    checked: modelData.customThresholds
                                    font.family: App.uiFont
                                    font.pixelSize: 12
                                    contentItem: Text {
                                        text: customTh.text
                                        color: Theme.textMuted
                                        font: customTh.font
                                        leftPadding: customTh.indicator.width + 8
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onToggled: App.setNodeThresholds(modelData.deviceId, checked, nmw.value, nmc.value, ntw.value, ntc.value)
                                }
                                IndustrialButton {
                                    visible: customTh.checked
                                    compact: true
                                    enabled: root.admin
                                    text: "Save limits"
                                    onClicked: App.setNodeThresholds(modelData.deviceId, true, nmw.value, nmc.value, ntw.value, ntc.value)
                                }

                                Grid {
                                    visible: customTh.checked
                                    columns: 2
                                    columnSpacing: 20
                                    rowSpacing: 8
                                    width: parent.width
                                    enabled: root.admin
                                    Column {
                                        spacing: 4
                                        Text { text: "Moisture warning  " + nmw.value.toFixed(1) + "%"; color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                                        Slider { id: nmw; width: 260; from: 5; to: 90; value: modelData.moistureWarning }
                                    }
                                    Column {
                                        spacing: 4
                                        Text { text: "Moisture critical  " + nmc.value.toFixed(1) + "%"; color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                                        Slider { id: nmc; width: 260; from: 10; to: 95; value: modelData.moistureCritical }
                                    }
                                    Column {
                                        spacing: 4
                                        Text { text: "Tilt warning  " + ntw.value.toFixed(1) + "°"; color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                                        Slider { id: ntw; width: 260; from: 1; to: 180; value: modelData.tiltWarning }
                                    }
                                    Column {
                                        spacing: 4
                                        Text { text: "Tilt critical  " + ntc.value.toFixed(1) + "°"; color: Theme.textMuted; font.pixelSize: 11; font.family: App.uiFont }
                                        Slider { id: ntc; width: 260; from: 2; to: 180; value: modelData.tiltCritical }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ---------------- Link ----------------
            Flickable {
                clip: true
                contentHeight: linkCol.height

                Column {
                    id: linkCol
                    width: Math.min(680, stack.width)
                    spacing: 18

                    // --- where the numbers on screen are actually coming from ---
                    Text {
                        text: "DATA SOURCE"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 4
                        color: App.connectionState === 4 ? "#1F1A12" : Theme.bgInset
                        border.width: 1
                        border.color: App.connectionState === 2 ? Theme.safe
                                    : (App.connectionState === 4 ? Theme.warning : Theme.border)

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 10
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: App.connectionState === 2 ? Theme.safe
                                     : (App.connectionState === 4 ? Theme.warning : Theme.textMuted)
                            }
                            Text {
                                text: App.sourceLabel
                                color: App.connectionState === 4 ? Theme.warning : Theme.text
                                font.family: App.uiFont
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    CheckBox {
                        id: simBox
                        enabled: root.admin
                        checked: App.simulationEnabled
                        onClicked: App.simulationEnabled = checked
                        text: "Use simulated telemetry (demo only — live port data is paused)"
                        font.family: App.uiFont
                        font.pixelSize: 12
                        contentItem: Text {
                            text: simBox.text
                            color: Theme.text
                            font: simBox.font
                            leftPadding: simBox.indicator.width + 8
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: "Off by default. When unchecked, the dashboard auto-opens the USB-UART receiver at 115200 8N1 and stores every decoded frame."
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 11
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }

                    // --- port selection, reachable without hovering the sidebar ---
                    Text {
                        text: "RECEIVER PORT"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    Row {
                        spacing: 10

                        ComboBox {
                            id: settingsPort
                            enabled: root.admin && App.connectionState !== 2
                            width: 220
                            height: 32
                            model: App.availablePorts
                            font.family: App.monoFont
                            font.pixelSize: 12
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
                                function onPortsChanged() { settingsPort.selectCurrent() }
                                function onConnectionChanged() { settingsPort.selectCurrent() }
                            }

                            background: Rectangle {
                                implicitHeight: 32
                                color: Theme.bgInset
                                border.color: settingsPort.popup.visible ? Theme.action : Theme.border
                                border.width: 1
                                radius: 4
                            }
                            contentItem: Text {
                                leftPadding: 10
                                rightPadding: 22
                                elide: Text.ElideRight
                                text: {
                                    if (!App.availablePorts.length)
                                        return "No serial ports detected"
                                    const i = settingsPort.currentIndex
                                    if (i >= 0 && i < App.availablePortLabels.length)
                                        return App.availablePortLabels[i]
                                    return settingsPort.displayText
                                }
                                color: App.availablePorts.length ? Theme.text : Theme.textDim
                                font: settingsPort.font
                                verticalAlignment: Text.AlignVCenter
                            }
                            popup: Popup {
                                y: settingsPort.height + 2
                                width: settingsPort.width
                                implicitHeight: Math.min(contentItem.implicitHeight + 2, 200)
                                padding: 1
                                background: Rectangle {
                                    color: Theme.bgCard
                                    border.color: Theme.border
                                    border.width: 1
                                    radius: 4
                                }
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: settingsPort.popup.visible ? settingsPort.delegateModel : null
                                    currentIndex: settingsPort.highlightedIndex
                                    ScrollIndicator.vertical: ScrollIndicator {}
                                }
                            }
                            delegate: ItemDelegate {
                                width: settingsPort.width
                                height: 28
                                highlighted: settingsPort.highlightedIndex === index
                                background: Rectangle { color: highlighted ? Theme.bgCard : "transparent" }
                                contentItem: Text {
                                    leftPadding: 10
                                    text: (index >= 0 && index < App.availablePortLabels.length)
                                          ? App.availablePortLabels[index] : modelData
                                    color: Theme.text
                                    font.family: App.monoFont
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        IndustrialButton {
                            text: "Rescan"
                            compact: true
                            onClicked: App.refreshPorts()
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        IndustrialButton {
                            enabled: root.admin
                            primary: App.connectionState !== 2
                            destructive: App.connectionState === 2
                            compact: true
                            text: App.connectionState === 2 ? "Disconnect"
                                : (App.connectionState === 1 ? "Connecting…" : "Connect")
                            onClicked: {
                                if (App.connectionState === 2)
                                    App.disconnectSerial()
                                else {
                                    App.refreshPorts()
                                    App.connectSerial()
                                }
                            }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        IndustrialButton {
                            enabled: App.connectionState === 2 && !App.dumpBusy
                            primary: true
                            compact: true
                            text: App.dumpBusy ? (App.dumpPhase.length ? App.dumpPhase : "Working…") : "Refresh"
                            onClicked: App.refreshFromReceiver()
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        IndustrialButton {
                            enabled: root.admin && App.connectionState === 2 && !App.dumpBusy
                            destructive: true
                            compact: true
                            text: "Clear log"
                            onClicked: App.clearReceiverLog()
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: "v1.0  ·  115200 8N1 big-endian  ·  dashboard sends one command at a time (0x53 status, 0x44 full dump, 0x4E + last RID, 0x43 clear). Receiver answers 0xAA + cmd + uint32 length. Refresh = 0x44. Clear log wipes the EEPROM and resets RID to 0."
                        color: Theme.textDim
                        font.family: App.monoFont
                        font.pixelSize: 11
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }

                    // --- proof that bytes are arriving and decoding ---
                    Text {
                        text: "LINK HEALTH"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    Grid {
                        columns: 4
                        columnSpacing: 12
                        rowSpacing: 10
                        width: parent.width

                        Repeater {
                            model: [
                                { k: "BYTES IN", v: App.rxBytes.toString() },
                                { k: "FRAMES OK", v: App.rxFrames.toString() },
                                { k: "BYTES DROPPED", v: App.rxDiscarded.toString() },
                                { k: "LAST FRAME", v: App.rxLastFrame },
                                { k: "RX RECORDS", v: App.receiverWriteCount.toString() },
                                { k: "LAST RID", v: App.hasLastRecordId ? App.lastRecordId.toString() : "—" },
                                { k: "FREE SLOTS", v: App.receiverFreeRecords.toString() },
                                { k: "DUMP", v: App.dumpBusy ? (App.dumpPhase + "  " + App.dumpPercent + "%") : "idle" }
                            ]
                            delegate: Rectangle {
                                width: (linkCol.width - 36) / 4
                                height: 52
                                radius: 4
                                color: Theme.bgInset
                                border.color: Theme.border
                                border.width: 1
                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 3
                                    Text {
                                        text: modelData.k
                                        color: Theme.textDim
                                        font.family: App.uiFont
                                        font.pixelSize: 9
                                        font.letterSpacing: 0.9
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: modelData.v
                                        color: Theme.text
                                        font.family: App.monoFont
                                        font.pixelSize: 15
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: diagText.implicitHeight + 20
                        radius: 4
                        color: Theme.bgInset
                        border.width: 1
                        border.color: App.connectionState === 2 && App.rxFrames > 0 ? Theme.safe
                                    : (App.connectionState === 2 ? Theme.warning : Theme.border)
                        Text {
                            id: diagText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            wrapMode: Text.WordWrap
                            text: App.linkDiagnosis
                            color: App.connectionState === 2 && App.rxFrames > 0 ? Theme.safe : Theme.textMuted
                            font.family: App.uiFont
                            font.pixelSize: 12
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        Text {
                            text: "PROTOCOL TRACE"
                            color: Theme.textDim
                            font.family: App.uiFont
                            font.pixelSize: 10
                            font.letterSpacing: 1.2
                            font.weight: Font.DemiBold
                        }
                        Rectangle {
                            width: parent.width
                            height: 6
                            radius: 3
                            color: Theme.borderSoft
                            visible: App.dumpBusy
                            Rectangle {
                                width: parent.width * App.dumpPercent / 100
                                height: parent.height
                                radius: 3
                                color: Theme.action
                            }
                        }
                        Rectangle {
                            width: parent.width
                            height: 168
                            radius: 4
                            color: "#0E1117"
                            border.color: App.dumpBusy ? Theme.action : Theme.border
                            border.width: 1
                            Flickable {
                                id: traceFlick
                                anchors.fill: parent
                                anchors.margins: 8
                                contentWidth: width
                                contentHeight: traceText.implicitHeight
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                Text {
                                    id: traceText
                                    width: traceFlick.width
                                    text: App.serialTrace.length ? App.serialTrace : "Connect the receiver — TX commands and RX headers will appear here."
                                    color: App.serialTrace.length ? Theme.text : Theme.textFaint
                                    wrapMode: Text.WrapAnywhere
                                    font.family: App.monoFont
                                    font.pixelSize: 11
                                    lineHeight: 1.35
                                    onTextChanged: Qt.callLater(function() {
                                        if (traceFlick.contentHeight > traceFlick.height)
                                            traceFlick.contentY = traceFlick.contentHeight - traceFlick.height
                                    })
                                }
                            }
                        }
                        Row {
                            spacing: 8
                            IndustrialButton {
                                compact: true
                                text: "Copy trace"
                                enabled: App.serialTrace.length > 0
                                onClicked: App.copyToClipboard(App.serialTrace)
                            }
                            IndustrialButton {
                                compact: true
                                text: "Clear trace"
                                enabled: App.serialTrace.length > 0
                                onClicked: App.clearSerialTrace()
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        visible: App.rxTail.length > 0
                        Text {
                            text: "LAST BYTES ON THE WIRE"
                            color: Theme.textDim
                            font.family: App.uiFont
                            font.pixelSize: 9
                            font.letterSpacing: 0.9
                            font.weight: Font.DemiBold
                        }
                        Rectangle {
                            width: parent.width
                            height: hexText.implicitHeight + 16
                            radius: 4
                            color: "#0E1117"
                            border.color: Theme.border
                            border.width: 1
                            Text {
                                id: hexText
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: App.rxTail
                                color: Theme.textMuted
                                wrapMode: Text.WrapAnywhere
                                font.family: App.monoFont
                                font.pixelSize: 11
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }

                    // --- local store ---
                    Text {
                        text: "LOCAL STORE"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    Grid {
                        columns: 2
                        columnSpacing: 40
                        rowSpacing: 8
                        width: parent.width

                        Text { text: "Engine"; color: Theme.textMuted; font.pixelSize: 12; font.family: App.uiFont }
                        Text { text: App.storeStatus; color: Theme.text; font.family: App.monoFont; font.pixelSize: 12 }

                        Text { text: "Stored readings"; color: Theme.textMuted; font.pixelSize: 12; font.family: App.uiFont }
                        Text { text: App.storedSamples + " rows  ·  30-day retention"; color: Theme.text; font.family: App.monoFont; font.pixelSize: 12 }

                        Text { text: "Stored alerts"; color: Theme.textMuted; font.pixelSize: 12; font.family: App.uiFont }
                        Text { text: App.storedAlerts + " rows"; color: Theme.text; font.family: App.monoFont; font.pixelSize: 12 }

                        Text { text: "Written this session"; color: Theme.textMuted; font.pixelSize: 12; font.family: App.uiFont }
                        Text {
                            text: App.storeRows + " rows  ·  " + App.storePending + " buffered"
                            color: App.storeRows > 0 ? Theme.safe : Theme.textMuted
                            font.family: App.monoFont
                            font.pixelSize: 12
                        }

                        Text { text: "Last write"; color: Theme.textMuted; font.pixelSize: 12; font.family: App.uiFont }
                        Text { text: App.storeLastWrite; color: Theme.text; font.family: App.monoFont; font.pixelSize: 12 }

                        Text { text: "File size"; color: Theme.textMuted; font.pixelSize: 12; font.family: App.uiFont }
                        Text { text: App.storeSize; color: Theme.text; font.family: App.monoFont; font.pixelSize: 12 }
                    }

                    Text {
                        width: parent.width
                        text: App.storePath
                        color: Theme.textDim
                        font.family: App.monoFont
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }

                    Row {
                        spacing: 10
                        IndustrialButton {
                            compact: true
                            text: "Copy path"
                            onClicked: App.copyToClipboard(App.storePath)
                        }
                        IndustrialButton {
                            compact: true
                            text: "Flush to disk now"
                            onClicked: App.flushStore()
                        }
                    }
                }
            }

            // ---------------- Access ----------------
            Flickable {
                clip: true
                contentHeight: accessCol.height
                Column {
                    id: accessCol
                    width: Math.min(660, stack.width)
                    spacing: 20

                    Text {
                        text: "SESSION"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    Row {
                        spacing: 14
                        Rectangle {
                            width: sessionRole.implicitWidth + 16
                            height: 22
                            radius: 3
                            color: root.admin ? Qt.rgba(Theme.action.r, Theme.action.g, Theme.action.b, 0.16) : Theme.bgInset
                            border.color: root.admin ? Theme.action : Theme.border
                            border.width: 1
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                id: sessionRole
                                anchors.centerIn: parent
                                text: Auth.roleLabel
                                color: root.admin ? Theme.action : Theme.textMuted
                                font.family: App.uiFont
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                font.letterSpacing: 0.9
                            }
                        }
                        Text {
                            text: "signed in as " + Auth.username
                            color: Theme.textMuted
                            font.family: App.uiFont
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        IndustrialButton {
                            destructive: true
                            compact: true
                            text: "Sign out"
                            onClicked: Auth.logout()
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.borderSoft }

                    // --- change own password ---
                    Text {
                        text: "CHANGE MY PASSWORD"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }
                    Row {
                        spacing: 12
                        FieldInput { id: curPass; width: 180; label: "CURRENT"; password: true }
                        FieldInput { id: newPass; width: 180; label: "NEW"; password: true }
                        FieldInput { id: confirmPass; width: 180; label: "CONFIRM"; password: true }
                    }
                    Row {
                        spacing: 12
                        IndustrialButton {
                            primary: true
                            text: "Update password"
                            onClicked: {
                                if (Auth.changeOwnPassword(curPass.text, newPass.text, confirmPass.text)) {
                                    curPass.text = ""; newPass.text = ""; confirmPass.text = ""
                                    passNotice.kind = "safe"
                                    passNotice.text = "Password updated"
                                } else {
                                    passNotice.kind = "critical"
                                    passNotice.text = Auth.errorMessage
                                }
                            }
                        }
                        MessageBanner {
                            id: passNotice
                            compact: true
                            kind: "safe"
                            text: ""
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.borderSoft; visible: root.admin }

                    // --- operator directory (admin only) ---
                    Text {
                        visible: root.admin
                        text: "OPERATORS"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    Column {
                        visible: root.admin
                        width: parent.width
                        spacing: 6

                        Repeater {
                            model: root.userRows
                            delegate: Rectangle {
                                width: accessCol.width
                                height: 40
                                radius: 4
                                color: Theme.bgInset
                                border.color: Theme.border
                                border.width: 1

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 14

                                    Text {
                                        width: 130
                                        height: parent.height
                                        text: modelData.username
                                        color: Theme.text
                                        font.family: App.monoFont
                                        font.pixelSize: 12
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    Item {
                                        width: 70
                                        height: parent.height
                                        Rectangle {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: roleTag.implicitWidth + 14
                                            height: 18
                                            radius: 3
                                            color: modelData.role === 2 ? Qt.rgba(Theme.action.r, Theme.action.g, Theme.action.b, 0.16) : Theme.bgInset
                                            border.color: modelData.role === 2 ? Theme.action : Theme.border
                                            border.width: 1
                                            Text {
                                                id: roleTag
                                                anchors.centerIn: parent
                                                text: modelData.roleLabel
                                                color: modelData.role === 2 ? Theme.action : Theme.textMuted
                                                font.family: App.uiFont
                                                font.pixelSize: 9
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                    }
                                    Text {
                                        width: 150
                                        height: parent.height
                                        text: "last: " + modelData.lastLogin
                                        color: Theme.textDim
                                        font.family: App.monoFont
                                        font.pixelSize: 11
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    MessageBanner {
                                        visible: modelData.defaultPassword
                                        compact: true
                                        kind: "warning"
                                        text: "DEFAULT PW"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                IndustrialButton {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    compact: true
                                    destructive: true
                                    text: "Remove"
                                    enabled: modelData.username !== "admin" && modelData.username !== Auth.username
                                    onClicked: Auth.removeUser(modelData.username)
                                }
                            }
                        }
                    }

                    // --- add operator ---
                    Column {
                        visible: root.admin
                        spacing: 12
                        Text {
                            text: "ADD OPERATOR"
                            color: Theme.textDim
                            font.family: App.uiFont
                            font.pixelSize: 10
                            font.letterSpacing: 1.2
                            font.weight: Font.DemiBold
                        }
                        Row {
                            spacing: 12
                            FieldInput { id: newUser; width: 180; label: "OPERATOR"; placeholder: "e.g. shift-b" }
                            FieldInput { id: newUserPass; width: 180; label: "PASSWORD"; password: true }
                            Column {
                                spacing: 6
                                Text {
                                    text: "ROLE"
                                    color: Theme.textDim
                                    font.family: App.uiFont
                                    font.pixelSize: 10
                                    font.letterSpacing: 1.1
                                    font.weight: Font.DemiBold
                                }
                                SegmentedControl {
                                    id: roleSeg
                                    model: ["GUEST", "ADMIN"]
                                    currentIndex: 0
                                }
                            }
                        }
                        Row {
                            spacing: 12
                            IndustrialButton {
                                primary: true
                                text: "Create operator"
                                onClicked: {
                                    if (Auth.createUser(newUser.text, newUserPass.text, roleSeg.currentIndex === 1 ? 2 : 1)) {
                                        newUser.text = ""; newUserPass.text = ""
                                        userNotice.kind = "safe"
                                        userNotice.text = "Operator added"
                                    } else {
                                        userNotice.kind = "critical"
                                        userNotice.text = Auth.errorMessage
                                    }
                                }
                            }
                            MessageBanner {
                                id: userNotice
                                compact: true
                                kind: "safe"
                                text: ""
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }
}
