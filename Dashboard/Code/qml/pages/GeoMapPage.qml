import QtQuick
import QtQuick.Controls

Item {
    id: root

    property int zoom: 5
    property real centerLat: 31.956
    property real centerLon: 35.923
    property real originX: 0
    property real originY: 0
    property bool satellite: false
    property bool fitted: false

    readonly property bool hasAnyGeo: {
        const nodes = App.mapNodes
        for (let i = 0; i < nodes.length; ++i) {
            if (nodes[i].hasGeo)
                return true
        }
        const zones = App.zones
        for (let i = 0; i < zones.length; ++i) {
            if (zones[i].hasCoverage)
                return true
        }
        return false
    }

    readonly property int tileSize: 256
    readonly property int minZoom: 3
    readonly property int maxZoom: 19

    function worldSize() {
        return tileSize * Math.pow(2, zoom)
    }
    function lonToWorldX(lon) {
        return (Number(lon) + 180) / 360 * worldSize()
    }
    function latToWorldY(lat) {
        const s = Math.sin(Number(lat) * Math.PI / 180)
        const clamped = Math.max(-0.9999, Math.min(0.9999, s))
        return (0.5 - Math.log((1 + clamped) / (1 - clamped)) / (4 * Math.PI)) * worldSize()
    }
    function lonToX(lon) { return lonToWorldX(lon) - originX }
    function latToY(lat) { return latToWorldY(lat) - originY }
    function xToLon(sx) { return (originX + sx) / worldSize() * 360 - 180 }
    function yToLat(sy) {
        const n = worldSize()
        const wy = originY + sy
        const m = Math.exp(Math.PI * (1 - 2 * wy / n))
        return Math.atan(m) * 2 * 180 / Math.PI - 90
    }

    property bool addOpen: false
    property real addLat: 0
    property real addLon: 0
    property string addError: ""

    property bool drawingPlace: false
    property bool drawActive: false
    property real drawX0: 0
    property real drawY0: 0
    property real drawX1: 0
    property real drawY1: 0
    property bool placeOpen: false
    property string placeError: ""
    property string hoverPlaceId: ""
    property int placingId: 0

    property bool editPlaceOpen: false
    property string editPlaceId: ""
    property real editLat1: 0
    property real editLon1: 0
    property real editLat2: 0
    property real editLon2: 0
    property real editX: 0
    property real editY: 0
    property real editW: 0
    property real editH: 0
    property real editAlt1: NaN
    property real editAlt2: NaN

    function openAddAt(sx, sy) {
        addLat = yToLat(sy)
        addLon = xToLon(sx)
        addError = ""
        addIdField.text = ""
        addOpen = true
        Qt.callLater(function() { addIdField.input.forceActiveFocus() })
    }

    function submitAdd() {
        const id = App.parseDeviceId(addIdField.text)
        if (id <= 0) {
            addError = "Enter a node ID such as 260 or NODE260"
            return
        }
        if (!Auth.isAdmin) {
            addError = "Admin sign-in is required to place a node"
            return
        }
        if (!App.addNodeAt(id, addLat, addLon)) {
            addError = "Could not place that node"
            return
        }
        addOpen = false
        fitted = true
    }

    function placeIdAtScreen(sx, sy) {
        const zones = App.zones
        let best = ""
        let bestA = 1e300
        for (let i = 0; i < zones.length; ++i) {
            const z = zones[i]
            if (!z.hasCoverage)
                continue
            const x = Math.min(root.lonToX(z.lon1), root.lonToX(z.lon2))
            const y = Math.min(root.latToY(z.lat1), root.latToY(z.lat2))
            const w = Math.abs(root.lonToX(z.lon2) - root.lonToX(z.lon1))
            const h = Math.abs(root.latToY(z.lat2) - root.latToY(z.lat1))
            if (sx >= x && sx <= x + w && sy >= y && sy <= y + h) {
                const a = Math.max(w * h, 1)
                if (a < bestA) {
                    bestA = a
                    best = z.id
                }
            }
        }
        return best
    }

    function beginDrawPlace() {
        if (!Auth.isAdmin)
            return
        drawingPlace = true
        drawActive = false
        placingId = 0
    }

    function cancelModes() {
        drawingPlace = false
        drawActive = false
        placingId = 0
        hoverPlaceId = ""
    }

    function openMapMenu(mx, my) {
        menuAnchor.x = mx
        menuAnchor.y = my
        mapMenu.popup()
    }

    function openNodeMenu(mx, my, deviceId) {
        nodeMenu.deviceId = deviceId
        menuAnchor.x = mx
        menuAnchor.y = my
        nodeMenu.popup()
    }

    function openPlaceMenu(mx, my, place) {
        placeMenu.placeId = place.id
        placeMenu.placeName = place.name
        placeMenu.notes = place.notes || ""
        placeMenu.lat1 = place.lat1
        placeMenu.lon1 = place.lon1
        placeMenu.lat2 = place.lat2
        placeMenu.lon2 = place.lon2
        placeMenu.alt1 = place.alt1
        placeMenu.alt2 = place.alt2
        placeMenu.px = place.x
        placeMenu.py = place.y
        placeMenu.pw = place.w
        placeMenu.ph = place.h
        menuAnchor.x = mx
        menuAnchor.y = my
        placeMenu.popup()
    }

    function finishDrawPlace() {
        drawingPlace = false
        drawActive = false
        const w = Math.abs(drawX1 - drawX0)
        const h = Math.abs(drawY1 - drawY0)
        if (w < 12 || h < 12)
            return
        const x0 = Math.min(drawX0, drawX1)
        const y0 = Math.min(drawY0, drawY1)
        const x1 = Math.max(drawX0, drawX1)
        const y1 = Math.max(drawY0, drawY1)
        editLat1 = yToLat(y0)
        editLon1 = xToLon(x0)
        editLat2 = yToLat(y1)
        editLon2 = xToLon(x1)
        placeNameField.text = ""
        placeNotesField.text = ""
        root.placeError = ""
        placeOpen = true
        Qt.callLater(function() { placeNameField.input.forceActiveFocus() })
    }

    function submitPlace() {
        const name = String(placeNameField.text).trim()
        if (name.length < 2) {
            root.placeError = "Enter a place name"
            return
        }
        if (!Auth.isAdmin)
            return
        const id = App.createPlaceCoverage(name, placeNotesField.text, editLat1, editLon1, editLat2, editLon2)
        if (!id || !id.length) {
            root.placeError = "Could not create place"
            return
        }
        placeOpen = false
        fitted = true
    }

    function openEditPlace(placeId) {
        const zones = App.zones
        for (let i = 0; i < zones.length; ++i) {
            if (zones[i].id !== placeId)
                continue
            const z = zones[i]
            editPlaceId = z.id
            editLat1 = z.lat1
            editLon1 = z.lon1
            editLat2 = z.lat2
            editLon2 = z.lon2
            editAlt1 = z.alt1
            editAlt2 = z.alt2
            editX = z.x
            editY = z.y
            editW = z.w
            editH = z.h
            editPlaceName.text = z.name
            editPlaceNotes.text = z.notes || ""
            editPlaceOpen = true
            return
        }
    }

    function submitEditPlace() {
        const name = String(editPlaceName.text).trim()
        if (name.length < 2)
            return
        App.updatePlace(editPlaceId, name, editPlaceNotes.text, editX, editY, editW, editH,
                        editLat1, editLon1, editAlt1, editLat2, editLon2, editAlt2)
        editPlaceOpen = false
    }

    function wrapTileX(x) {
        const n = Math.pow(2, zoom)
        return ((x % n) + n) % n
    }

    function tileUrl(tx, ty) {
        const x = wrapTileX(tx)
        const y = ty
        if (satellite)
            return "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/"
                    + zoom + "/" + y + "/" + x
        return "https://tile.openstreetmap.org/" + zoom + "/" + x + "/" + y + ".png"
    }

    function syncOriginFromCenter() {
        originX = lonToWorldX(centerLon) - width / 2
        originY = latToWorldY(centerLat) - height / 2
        rebuildTiles()
    }

    function syncCenterFromOrigin() {
        const wx = originX + width / 2
        const wy = originY + height / 2
        const n = worldSize()
        centerLon = wx / n * 360 - 180
        const m = Math.exp(Math.PI * (1 - 2 * wy / n) )
        centerLat = Math.atan(m) * 2 * 180 / Math.PI - 90
    }

    function rebuildTiles() {
        if (width < 8 || height < 8)
            return
        const n = Math.pow(2, zoom)
        const minTx = Math.floor(originX / tileSize) - 1
        const minTy = Math.max(0, Math.floor(originY / tileSize) - 1)
        const maxTx = Math.floor((originX + width) / tileSize) + 1
        const maxTy = Math.min(n - 1, Math.floor((originY + height) / tileSize) + 1)
        tileModel.clear()
        for (let ty = minTy; ty <= maxTy; ++ty) {
            for (let tx = minTx; tx <= maxTx; ++tx)
                tileModel.append({ tx: tx, ty: ty })
        }
    }

    function fitToData() {
        let minLat = 90, maxLat = -90, minLon = 180, maxLon = -180
        let any = false
        const nodes = App.mapNodes
        for (let i = 0; i < nodes.length; ++i) {
            if (!nodes[i].hasGeo)
                continue
            any = true
            minLat = Math.min(minLat, nodes[i].latitude)
            maxLat = Math.max(maxLat, nodes[i].latitude)
            minLon = Math.min(minLon, nodes[i].longitude)
            maxLon = Math.max(maxLon, nodes[i].longitude)
        }
        const zones = App.zones
        for (let i = 0; i < zones.length; ++i) {
            if (!zones[i].hasCoverage)
                continue
            any = true
            minLat = Math.min(minLat, zones[i].lat1, zones[i].lat2)
            maxLat = Math.max(maxLat, zones[i].lat1, zones[i].lat2)
            minLon = Math.min(minLon, zones[i].lon1, zones[i].lon2)
            maxLon = Math.max(maxLon, zones[i].lon1, zones[i].lon2)
        }
        if (!any) {
            if (zoom < 3)
                zoom = 4
            syncOriginFromCenter()
            return
        }
        if (minLat === maxLat) { minLat -= 0.01; maxLat += 0.01 }
        if (minLon === maxLon) { minLon -= 0.01; maxLon += 0.01 }
        centerLat = (minLat + maxLat) / 2
        centerLon = (minLon + maxLon) / 2
        let z = maxZoom
        for (; z >= minZoom; --z) {
            const span = tileSize * Math.pow(2, z)
            const w = ((maxLon - minLon) / 360) * span
            const y1 = (0.5 - Math.log((1 + Math.sin(maxLat * Math.PI / 180)) / (1 - Math.sin(maxLat * Math.PI / 180))) / (4 * Math.PI)) * span
            const y2 = (0.5 - Math.log((1 + Math.sin(minLat * Math.PI / 180)) / (1 - Math.sin(minLat * Math.PI / 180))) / (4 * Math.PI)) * span
            if (w < width - 96 && Math.abs(y2 - y1) < height - 96)
                break
        }
        zoom = z
        fitted = true
        syncOriginFromCenter()
    }

    onWidthChanged: { if (visible && width > 8) syncOriginFromCenter() }
    onHeightChanged: { if (visible && height > 8) syncOriginFromCenter() }
    onZoomChanged: { if (visible) syncOriginFromCenter() }
    onSatelliteChanged: { if (visible) rebuildTiles() }
    onVisibleChanged: {
        if (visible)
            Qt.callLater(function() {
                if (hasAnyGeo)
                    fitToData()
                else
                    syncOriginFromCenter()
            })
    }

    Connections {
        target: App
        function onMapNodesChanged() {
            if (root.visible && !root.fitted && root.hasAnyGeo)
                root.fitToData()
        }
        function onPageChanged() {
            if (App.page === 2)
                Qt.callLater(function() {
                    if (root.hasAnyGeo)
                        root.fitToData()
                    else
                        root.syncOriginFromCenter()
                })
        }
    }

    Component.onCompleted: Qt.callLater(function() {
        if (visible)
            syncOriginFromCenter()
    })

    ListModel { id: tileModel }

    Rectangle {
        id: mapClip
        anchors.fill: parent
        color: satellite ? "#0B1A12" : "#C8D2C8"
        clip: true

        Repeater {
            model: tileModel
            delegate: Image {
                x: model.tx * root.tileSize - root.originX
                y: model.ty * root.tileSize - root.originY
                width: root.tileSize
                height: root.tileSize
                asynchronous: true
                cache: true
                fillMode: Image.PreserveAspectCrop
                source: root.tileUrl(model.tx, model.ty)
            }
        }

        MouseArea {
            id: pan
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            property real lastX: 0
            property real lastY: 0
            property bool dragging: false
            cursorShape: {
                if (root.drawingPlace || root.placingId > 0)
                    return Qt.CrossCursor
                return dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            }
            onPressed: function(mouse) {
                if (mouse.button === Qt.RightButton)
                    return
                lastX = mouse.x
                lastY = mouse.y
                dragging = false
                if (root.drawingPlace) {
                    root.drawX0 = mouse.x
                    root.drawY0 = mouse.y
                    root.drawX1 = mouse.x
                    root.drawY1 = mouse.y
                    root.drawActive = true
                }
            }
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    if (root.drawingPlace || root.placingId > 0) {
                        root.cancelModes()
                        return
                    }
                    root.openMapMenu(mouse.x, mouse.y)
                    return
                }
                if (root.placingId > 0 && Auth.isAdmin) {
                    const placeId = root.placeIdAtScreen(mouse.x, mouse.y)
                    App.moveNodeOnMap(root.placingId, root.yToLat(mouse.y), root.xToLon(mouse.x), placeId)
                    root.placingId = 0
                }
            }
            onPositionChanged: function(mouse) {
                if (!pressed || (mouse.buttons & Qt.LeftButton) === 0)
                    return
                if (root.drawingPlace && root.drawActive) {
                    root.drawX1 = mouse.x
                    root.drawY1 = mouse.y
                    return
                }
                const dx = mouse.x - lastX
                const dy = mouse.y - lastY
                if (!dragging && Math.abs(dx) + Math.abs(dy) < 4)
                    return
                dragging = true
                originX -= dx
                originY -= dy
                lastX = mouse.x
                lastY = mouse.y
                syncCenterFromOrigin()
                rebuildTiles()
            }
            onReleased: function(mouse) {
                if (root.drawingPlace && root.drawActive && mouse.button === Qt.LeftButton)
                    root.finishDrawPlace()
                dragging = false
            }
            onWheel: function(w) {
                const oldZ = root.zoom
                const next = Math.max(root.minZoom, Math.min(root.maxZoom, oldZ + (w.angleDelta.y > 0 ? 1 : -1)))
                if (next === oldZ)
                    return
                const wx = root.originX + w.x
                const wy = root.originY + w.y
                const scale = Math.pow(2, next - oldZ)
                root.zoom = next
                root.originX = wx * scale - w.x
                root.originY = wy * scale - w.y
                root.syncCenterFromOrigin()
                root.rebuildTiles()
            }
        }

        Repeater {
            model: App.zones
            delegate: Rectangle {
                id: zoneRect
                visible: modelData.hasCoverage
                x: Math.min(root.lonToX(modelData.lon1), root.lonToX(modelData.lon2))
                y: Math.min(root.latToY(modelData.lat1), root.latToY(modelData.lat2))
                width: Math.abs(root.lonToX(modelData.lon2) - root.lonToX(modelData.lon1))
                height: Math.abs(root.latToY(modelData.lat2) - root.latToY(modelData.lat1))
                color: Qt.rgba(Theme.action.r, Theme.action.g, Theme.action.b,
                               root.hoverPlaceId === modelData.id ? 0.28 : 0.12)
                border.color: modelData.color || Theme.action
                border.width: root.hoverPlaceId === modelData.id ? 3 : 2
                radius: 2
                z: 2
                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 6
                    text: modelData.name + (modelData.coverage ? ("\n" + modelData.coverage) : "")
                    color: Theme.text
                    style: Text.Outline
                    styleColor: "#CC12161C"
                    font.family: App.uiFont
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton)
                            root.openPlaceMenu(zoneRect.x + mouse.x, zoneRect.y + mouse.y, modelData)
                    }
                }
            }
        }

        Repeater {
            model: App.mapNodes
            delegate: Item {
                id: nodeItem
                visible: modelData.hasGeo
                width: 1
                height: 1
                property real dragDx: 0
                property real dragDy: 0
                x: root.lonToX(modelData.longitude) + dragDx
                y: root.latToY(modelData.latitude) + dragDy
                z: 3

                Rectangle {
                    id: pin
                    width: 16
                    height: 16
                    radius: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.verticalCenter
                    color: Theme.statusColor(modelData.status)
                    border.color: "#12161C"
                    border.width: 2
                }
                Text {
                    anchors.left: pin.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: pin.verticalCenter
                    text: (modelData.locationName && modelData.locationName.length
                           ? modelData.locationName + "  ·  " : "")
                          + modelData.idString
                          + (isFinite(modelData.altitude) ? ("  " + Number(modelData.altitude).toFixed(0) + " m") : "")
                    color: Theme.text
                    style: Text.Outline
                    styleColor: "#CC12161C"
                    font.family: App.monoFont
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: pinMouse
                    anchors.fill: pin
                    anchors.margins: -10
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Auth.isAdmin ? Qt.SizeAllCursor : Qt.PointingHandCursor
                    property bool dragging: false
                    onPressed: function(mouse) {
                        dragging = false
                        if (mouse.button === Qt.RightButton)
                            mouse.accepted = true
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed || (mouse.buttons & Qt.LeftButton) === 0 || !Auth.isAdmin)
                            return
                        const p = mapToItem(mapClip, mouse.x, mouse.y)
                        const dx = p.x - root.lonToX(modelData.longitude)
                        const dy = p.y - root.latToY(modelData.latitude)
                        if (!dragging && Math.abs(dx) + Math.abs(dy) < 5)
                            return
                        dragging = true
                        nodeItem.dragDx = dx
                        nodeItem.dragDy = dy
                        root.hoverPlaceId = root.placeIdAtScreen(p.x, p.y)
                    }
                    onReleased: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            root.openNodeMenu(nodeItem.x, nodeItem.y, modelData.deviceId)
                            dragging = false
                            return
                        }
                        if (dragging && Auth.isAdmin) {
                            const nx = root.lonToX(modelData.longitude) + nodeItem.dragDx
                            const ny = root.latToY(modelData.latitude) + nodeItem.dragDy
                            App.moveNodeOnMap(modelData.deviceId, root.yToLat(ny), root.xToLon(nx),
                                              root.placeIdAtScreen(nx, ny))
                        } else if (mouse.button === Qt.LeftButton) {
                            App.openDetail(modelData.deviceId)
                        }
                        nodeItem.dragDx = 0
                        nodeItem.dragDy = 0
                        dragging = false
                        root.hoverPlaceId = ""
                    }
                    onCanceled: {
                        nodeItem.dragDx = 0
                        nodeItem.dragDy = 0
                        dragging = false
                        root.hoverPlaceId = ""
                    }
                }
            }
        }

        Rectangle {
            visible: root.drawActive
            x: Math.min(root.drawX0, root.drawX1)
            y: Math.min(root.drawY0, root.drawY1)
            width: Math.abs(root.drawX1 - root.drawX0)
            height: Math.abs(root.drawY1 - root.drawY0)
            color: Qt.rgba(Theme.safe.r, Theme.safe.g, Theme.safe.b, 0.16)
            border.color: Theme.safe
            border.width: 2
            radius: 2
            z: 8
        }

        Item {
            id: menuAnchor
            width: 1
            height: 1
            z: 40

            Menu {
                id: mapMenu
                padding: 6
                background: Rectangle {
                    implicitWidth: 196
                    color: Theme.bgElevated
                    border.color: Theme.border
                    radius: 8
                }
                delegate: MenuItem {
                    implicitHeight: 32
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? Theme.text : Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    background: Rectangle {
                        color: parent.highlighted ? Theme.bgCardHover : "transparent"
                        radius: 6
                    }
                }
                Action {
                    text: "Add Node"
                    onTriggered: root.openAddAt(menuAnchor.x, menuAnchor.y)
                }
                Action {
                    text: "Add Place"
                    enabled: Auth.isAdmin
                    onTriggered: root.beginDrawPlace()
                }
            }

            Menu {
                id: nodeMenu
                property int deviceId: 0
                padding: 6
                background: Rectangle {
                    implicitWidth: 196
                    color: Theme.bgElevated
                    border.color: Theme.border
                    radius: 8
                }
                delegate: MenuItem {
                    implicitHeight: 32
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? Theme.text : Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    background: Rectangle {
                        color: parent.highlighted ? Theme.bgCardHover : "transparent"
                        radius: 6
                    }
                }
                Action {
                    text: "Edit details"
                    onTriggered: App.openDetail(nodeMenu.deviceId)
                }
                Action {
                    text: "Move Node"
                    enabled: Auth.isAdmin
                    onTriggered: root.placingId = nodeMenu.deviceId
                }
                Action {
                    text: "Delete Node"
                    enabled: Auth.isAdmin
                    onTriggered: App.deleteNode(nodeMenu.deviceId)
                }
            }

            Menu {
                id: placeMenu
                property string placeId: ""
                property string placeName: ""
                property string notes: ""
                property real lat1: 0
                property real lon1: 0
                property real lat2: 0
                property real lon2: 0
                property real alt1: NaN
                property real alt2: NaN
                property real px: 0
                property real py: 0
                property real pw: 0.28
                property real ph: 0.24
                padding: 6
                background: Rectangle {
                    implicitWidth: 196
                    color: Theme.bgElevated
                    border.color: Theme.border
                    radius: 8
                }
                delegate: MenuItem {
                    implicitHeight: 32
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? Theme.text : Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    background: Rectangle {
                        color: parent.highlighted ? Theme.bgCardHover : "transparent"
                        radius: 6
                    }
                }
                Action {
                    text: "Edit Place"
                    enabled: Auth.isAdmin
                    onTriggered: root.openEditPlace(placeMenu.placeId)
                }
                Action {
                    text: "Add Node"
                    onTriggered: root.openAddAt(menuAnchor.x, menuAnchor.y)
                }
                Action {
                    text: "Add Place"
                    enabled: Auth.isAdmin
                    onTriggered: root.beginDrawPlace()
                }
            }
        }
    }

    Text {
        visible: !root.hasAnyGeo
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 52
        text: "Map is live — right-click to add a node or draw a place."
        color: Theme.text
        style: Text.Outline
        styleColor: "#AA12161C"
        font.family: App.uiFont
        font.pixelSize: 13
        z: 5
    }

    Row {
        x: 18
        y: 14
        spacing: 8
        z: 6

        Text {
            text: "MAP VIEW  ·  WGS-84"
            color: Theme.text
            style: Text.Outline
            styleColor: "#AA12161C"
            font.family: App.uiFont
            font.pixelSize: 10
            font.letterSpacing: 1.0
            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                id: streetsBtn
                width: streetsLab.implicitWidth + 16
                height: 24
                radius: 4
                color: !root.satellite ? Theme.action : "transparent"
                border.width: 0
                Text {
                    id: streetsLab
                    anchors.centerIn: parent
                    text: "Streets"
                    color: !root.satellite ? Theme.actionInk : Theme.text
                    style: Text.Outline
                    styleColor: streetsMouse.containsMouse && root.satellite ? "#CC12161C" : "transparent"
                    font.family: App.uiFont
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: streetsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.satellite = false
                }
            }
            Rectangle {
                id: satBtn
                width: satLab.implicitWidth + 16
                height: 24
                radius: 4
                color: root.satellite ? Theme.action : "transparent"
                border.width: 0
                Text {
                    id: satLab
                    anchors.centerIn: parent
                    text: "Satellite"
                    color: root.satellite ? Theme.actionInk : Theme.text
                    style: Text.Outline
                    styleColor: satMouse.containsMouse && !root.satellite ? "#CC12161C" : "transparent"
                    font.family: App.uiFont
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: satMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.satellite = true
                }
            }
            Rectangle {
                id: fitBtn
                width: fitLab.implicitWidth + 16
                height: 24
                radius: 4
                color: "transparent"
                border.width: 0
                Text {
                    id: fitLab
                    anchors.centerIn: parent
                    text: "Fit"
                    color: Theme.text
                    style: Text.Outline
                    styleColor: fitMouse.containsMouse ? "#CC12161C" : "transparent"
                    font.family: App.uiFont
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: fitMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.fitToData()
                }
            }
        }
    }

    Column {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        spacing: 6
        z: 6
        IndustrialButton {
            compact: true
            text: "+"
            onClicked: root.zoom = Math.min(root.maxZoom, root.zoom + 1)
        }
        IndustrialButton {
            compact: true
            text: "−"
            onClicked: root.zoom = Math.max(root.minZoom, root.zoom - 1)
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 10
        z: 6
        text: (root.satellite
              ? "Imagery © Esri"
              : "© OpenStreetMap contributors")
              + "  ·  drag to pan  ·  drag a node onto a place to assign  ·  right-click for options"
        color: Theme.textDim
        font.family: App.uiFont
        font.pixelSize: 10
    }

    Rectangle {
        visible: root.addOpen
        anchors.fill: parent
        color: "#B012161C"
        z: 30

        MouseArea {
            anchors.fill: parent
            onClicked: root.addOpen = false
        }

        Rectangle {
            width: 360
            implicitHeight: addCol.implicitHeight + 36
            anchors.centerIn: parent
            radius: 8
            color: Theme.bgCard
            border.color: Theme.border
            border.width: 1

            MouseArea {
                anchors.fill: parent
                onClicked: {}
            }

            Column {
                id: addCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 18
                spacing: 12

                Text {
                    text: "ADD NODE"
                    color: Theme.textDim
                    font.family: App.uiFont
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Latitude  " + Number(root.addLat).toFixed(6)
                          + "\nLongitude  " + Number(root.addLon).toFixed(6)
                    color: Theme.text
                    font.family: App.monoFont
                    font.pixelSize: 13
                }
                FieldInput {
                    id: addIdField
                    width: parent.width
                    label: "NODE ID"
                    placeholder: "NODE260 or 260"
                    onSubmitted: root.submitAdd()
                }

                Flow {
                    width: parent.width
                    spacing: 6
                    visible: {
                        const nodes = App.mapNodes
                        for (let i = 0; i < nodes.length; ++i) {
                            if (!nodes[i].hasGeo)
                                return true
                        }
                        return false
                    }
                    Text {
                        text: "Unplaced"
                        color: Theme.textDim
                        font.family: App.uiFont
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        height: 22
                        verticalAlignment: Text.AlignVCenter
                    }
                    Repeater {
                        model: App.mapNodes
                        delegate: Rectangle {
                            visible: !modelData.hasGeo
                            width: pickLab.implicitWidth + 12
                            height: 22
                            radius: 3
                            color: Theme.bgInset
                            border.color: Theme.border
                            border.width: 1
                            Text {
                                id: pickLab
                                anchors.centerIn: parent
                                text: modelData.idString
                                color: Theme.text
                                font.family: App.monoFont
                                font.pixelSize: 11
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: addIdField.text = String(modelData.deviceId)
                            }
                        }
                    }
                }

                Text {
                    visible: root.addError.length > 0
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: root.addError
                    color: Theme.critical
                    font.family: App.uiFont
                    font.pixelSize: 11
                }

                Row {
                    spacing: 8
                    IndustrialButton {
                        compact: true
                        text: "Cancel"
                        onClicked: root.addOpen = false
                    }
                    IndustrialButton {
                        compact: true
                        primary: true
                        enabled: Auth.isAdmin
                        text: "Place node"
                        onClicked: root.submitAdd()
                    }
                }
            }
        }
    }

    Text {
        visible: root.drawingPlace || root.placingId > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 52
        z: 12
        text: root.drawingPlace
              ? "Drag to cover a place  ·  right-click to cancel"
              : "Click the map to place the node  ·  right-click to cancel"
        color: Theme.text
        style: Text.Outline
        styleColor: "#AA12161C"
        font.family: App.uiFont
        font.pixelSize: 13
        font.weight: Font.Medium
    }

    Rectangle {
        visible: root.placeOpen
        anchors.fill: parent
        color: "#B012161C"
        z: 30

        MouseArea {
            anchors.fill: parent
            onClicked: root.placeOpen = false
        }

        Rectangle {
            width: 360
            implicitHeight: placeCol.implicitHeight + 36
            anchors.centerIn: parent
            radius: 8
            color: Theme.bgCard
            border.color: Theme.border
            border.width: 1
            MouseArea { anchors.fill: parent; onClicked: {} }

            Column {
                id: placeCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 18
                spacing: 12

                Text {
                    text: "ADD PLACE"
                    color: Theme.textDim
                    font.family: App.uiFont
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Coverage  " + Number(Math.min(root.editLat1, root.editLat2)).toFixed(5)
                          + " … " + Number(Math.max(root.editLat1, root.editLat2)).toFixed(5)
                          + "\n          " + Number(Math.min(root.editLon1, root.editLon2)).toFixed(5)
                          + " … " + Number(Math.max(root.editLon1, root.editLon2)).toFixed(5)
                    color: Theme.text
                    font.family: App.monoFont
                    font.pixelSize: 12
                }
                FieldInput {
                    id: placeNameField
                    width: parent.width
                    label: "NAME"
                    placeholder: "e.g. North Slope"
                    onSubmitted: root.submitPlace()
                }
                FieldInput {
                    id: placeNotesField
                    width: parent.width
                    label: "DESCRIPTION"
                    placeholder: "optional"
                }
                Text {
                    visible: root.placeError.length > 0
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: root.placeError
                    color: Theme.critical
                    font.family: App.uiFont
                    font.pixelSize: 11
                }
                Row {
                    spacing: 8
                    IndustrialButton { compact: true; text: "Cancel"; onClicked: root.placeOpen = false }
                    IndustrialButton {
                        compact: true
                        primary: true
                        enabled: Auth.isAdmin
                        text: "Create place"
                        onClicked: root.submitPlace()
                    }
                }
            }
        }
    }

    Rectangle {
        visible: root.editPlaceOpen
        anchors.fill: parent
        color: "#B012161C"
        z: 30

        MouseArea {
            anchors.fill: parent
            onClicked: root.editPlaceOpen = false
        }

        Rectangle {
            width: 360
            implicitHeight: editPlaceCol.implicitHeight + 36
            anchors.centerIn: parent
            radius: 8
            color: Theme.bgCard
            border.color: Theme.border
            border.width: 1
            MouseArea { anchors.fill: parent; onClicked: {} }

            Column {
                id: editPlaceCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 18
                spacing: 12

                Text {
                    text: "EDIT PLACE"
                    color: Theme.textDim
                    font.family: App.uiFont
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }
                FieldInput {
                    id: editPlaceName
                    width: parent.width
                    label: "NAME"
                    onSubmitted: root.submitEditPlace()
                }
                FieldInput {
                    id: editPlaceNotes
                    width: parent.width
                    label: "DESCRIPTION"
                    placeholder: "optional"
                }
                Row {
                    spacing: 8
                    IndustrialButton { compact: true; text: "Cancel"; onClicked: root.editPlaceOpen = false }
                    IndustrialButton {
                        compact: true
                        primary: true
                        enabled: Auth.isAdmin
                        text: "Save"
                        onClicked: root.submitEditPlace()
                    }
                }
            }
        }
    }
}
