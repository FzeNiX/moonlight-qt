import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.2

import StreamingPreferences 1.0

// Per-host settings: preferred connection address and an optional
// stream settings profile that overrides the global settings for this host.
NavigableDialog {
    id: hostSettingsDialog

    property int pcIndex: -1
    property var hostSettings: ({})

    // Must match NvComputer::PreferredAddressType
    readonly property int addrAuto: 0
    readonly property int addrLocal: 1
    readonly property int addrRemote: 2
    readonly property int addrIpv6: 3
    readonly property int addrManual: 4

    readonly property var resolutionPresets: [
        { text: "1280x720 (720p)", w: 1280, h: 720 },
        { text: "1280x800 (16:10)", w: 1280, h: 800 },
        { text: "1920x1080 (1080p)", w: 1920, h: 1080 },
        { text: "1920x1200 (16:10)", w: 1920, h: 1200 },
        { text: "2560x1080 (21:9)", w: 2560, h: 1080 },
        { text: "2560x1440 (1440p)", w: 2560, h: 1440 },
        { text: "2560x1600 (16:10)", w: 2560, h: 1600 },
        { text: "3440x1440 (21:9)", w: 3440, h: 1440 },
        { text: "3840x2160 (4K)", w: 3840, h: 2160 },
        { text: "5120x1440 (32:9)", w: 5120, h: 1440 },
    ]

    title: qsTr("Settings for %1").arg(hostSettings.name ? hostSettings.name : "")
    standardButtons: Dialog.Save | Dialog.Cancel

    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 40 : 640, 640)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 40 : 720, 720)

    function openForPc(index) {
        pcIndex = index
        hostSettings = computerModel.getHostSettings(index)
        loadValues()
        open()
    }

    function addressText(label, addr) {
        return addr ? qsTr("%1: %2").arg(label).arg(addr) : qsTr("%1: (not known yet)").arg(label)
    }

    function selectByValue(comboBox, model, value) {
        for (var i = 0; i < model.count; i++) {
            if (model.get(i).val === value) {
                comboBox.currentIndex = i
                return
            }
        }
        comboBox.currentIndex = 0
    }

    function loadValues() {
        // Preferred address
        addressModel.clear()
        addressModel.append({ text: qsTr("Automatic (last working address, then local first)"), val: addrAuto })
        addressModel.append({ text: addressText(qsTr("Local network"), hostSettings.localAddress), val: addrLocal })
        addressModel.append({ text: addressText(qsTr("Remote / public IP"), hostSettings.remoteAddress), val: addrRemote })
        addressModel.append({ text: addressText(qsTr("IPv6"), hostSettings.ipv6Address), val: addrIpv6 })
        addressModel.append({ text: addressText(qsTr("Manually added address / DDNS"), hostSettings.manualAddress), val: addrManual })
        selectByValue(addressComboBox, addressModel, hostSettings.preferredAddress)

        // Start from the global settings, then apply any saved overrides
        var p = {
            width: StreamingPreferences.width,
            height: StreamingPreferences.height,
            fps: StreamingPreferences.fps,
            windowMode: StreamingPreferences.windowMode,
            enableHdr: StreamingPreferences.enableHdr,
            absoluteMouseMode: StreamingPreferences.absoluteMouseMode,
            controllerTypeOverride: StreamingPreferences.controllerTypeOverride,
            useVirtualDisplay: StreamingPreferences.useVirtualDisplay,
            renderScale: StreamingPreferences.renderScale,
            videoSharpening: StreamingPreferences.videoSharpening,
            videoSharpness: StreamingPreferences.videoSharpness,
            colorBrightness: StreamingPreferences.colorBrightness,
            colorContrast: StreamingPreferences.colorContrast,
            colorSaturation: StreamingPreferences.colorSaturation,
            showPerformanceOverlay: StreamingPreferences.showPerformanceOverlay,
        }
        var saved = hostSettings.profile || {}
        var hasSavedProfile = Object.keys(saved).length > 0
        for (var key in saved) {
            p[key] = saved[key]
        }

        customProfileCheck.checked = hostSettings.profileEnabled === true

        // Resolution
        resolutionComboBox.currentIndex = resolutionPresets.length // Custom
        for (var i = 0; i < resolutionPresets.length; i++) {
            if (resolutionPresets[i].w === Number(p.width) && resolutionPresets[i].h === Number(p.height)) {
                resolutionComboBox.currentIndex = i
                break
            }
        }
        customWidthField.text = p.width
        customHeightField.text = p.height

        fpsSpinBox.value = Number(p.fps)

        // A saved profile without a bitrate means "automatic"
        autoBitrateCheck.checked = !hasSavedProfile || saved.bitrateKbps === undefined
        bitrateSlider.value = saved.bitrateKbps !== undefined ? Number(saved.bitrateKbps) : StreamingPreferences.bitrateKbps

        selectByValue(windowModeComboBox, windowModeModel, Number(p.windowMode))
        hdrCheck.checked = p.enableHdr === true || p.enableHdr === "true"
        mouseCheck.checked = p.absoluteMouseMode === true || p.absoluteMouseMode === "true"
        selectByValue(controllerTypeComboBox, controllerTypeModel, Number(p.controllerTypeOverride))
        virtualDisplayCheck.checked = p.useVirtualDisplay === true || p.useVirtualDisplay === "true"
        selectByValue(renderScaleComboBox, renderScaleModel, Number(p.renderScale))
        sharpenCheck.checked = p.videoSharpening === true || p.videoSharpening === "true"
        sharpnessSlider.value = Number(p.videoSharpness)
        brightnessSlider.value = Number(p.colorBrightness)
        contrastSlider.value = Number(p.colorContrast)
        saturationSlider.value = Number(p.colorSaturation)
        perfOverlayCheck.checked = p.showPerformanceOverlay === true || p.showPerformanceOverlay === "true"
    }

    function selectedWidth() {
        return resolutionComboBox.currentIndex < resolutionPresets.length ?
                    resolutionPresets[resolutionComboBox.currentIndex].w : parseInt(customWidthField.text)
    }

    function selectedHeight() {
        return resolutionComboBox.currentIndex < resolutionPresets.length ?
                    resolutionPresets[resolutionComboBox.currentIndex].h : parseInt(customHeightField.text)
    }

    onAccepted: {
        computerModel.setPreferredAddress(pcIndex, addressModel.get(addressComboBox.currentIndex).val)

        var profile = {
            width: selectedWidth(),
            height: selectedHeight(),
            fps: Math.round(fpsSpinBox.value),
            windowMode: Math.round(windowModeModel.get(windowModeComboBox.currentIndex).val),
            enableHdr: hdrCheck.checked,
            absoluteMouseMode: mouseCheck.checked,
            controllerTypeOverride: Math.round(controllerTypeModel.get(controllerTypeComboBox.currentIndex).val),
            useVirtualDisplay: virtualDisplayCheck.checked,
            renderScale: Math.round(renderScaleModel.get(renderScaleComboBox.currentIndex).val),
            videoSharpening: sharpenCheck.checked,
            videoSharpness: Math.round(sharpnessSlider.value),
            colorBrightness: Math.round(brightnessSlider.value),
            colorContrast: Math.round(contrastSlider.value),
            colorSaturation: Math.round(saturationSlider.value),
            showPerformanceOverlay: perfOverlayCheck.checked,
        }
        if (!autoBitrateCheck.checked) {
            profile.bitrateKbps = Math.round(bitrateSlider.value)
        }

        // The profile values are kept even when disabled, so re-enabling restores them
        computerModel.setHostProfile(pcIndex, customProfileCheck.checked, profile)
    }

    ListModel { id: addressModel }

    ListModel {
        id: controllerTypeModel
        ListElement { text: qsTr("Automatic"); val: StreamingPreferences.CTO_AUTO }
        ListElement { text: qsTr("Xbox"); val: StreamingPreferences.CTO_XBOX }
        ListElement { text: qsTr("PlayStation (DualShock 4)"); val: StreamingPreferences.CTO_PLAYSTATION }
    }

    ListModel {
        id: renderScaleModel
        ListElement { text: "50%"; val: 50 }
        ListElement { text: "75%"; val: 75 }
        ListElement { text: "100%"; val: 100 }
        ListElement { text: "125%"; val: 125 }
        ListElement { text: "150%"; val: 150 }
        ListElement { text: "175%"; val: 175 }
        ListElement { text: "200%"; val: 200 }
    }

    ListModel {
        id: windowModeModel
        ListElement { text: qsTr("Fullscreen"); val: StreamingPreferences.WM_FULLSCREEN }
        ListElement { text: qsTr("Borderless windowed"); val: StreamingPreferences.WM_FULLSCREEN_DESKTOP }
        ListElement { text: qsTr("Windowed"); val: StreamingPreferences.WM_WINDOWED }
    }

    contentItem: ScrollView {
        id: scrollView
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            // Leave room for the vertical scroll bar
            width: scrollView.availableWidth - 16
            spacing: 8

            // ---- Connection ----
            Label {
                text: qsTr("Preferred address")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("This address is tried first when connecting. If it doesn't respond, Moonlight falls back to the other known addresses.")
                font.pointSize: 9
                wrapMode: Text.Wrap
            }

            ComboBox {
                id: addressComboBox
                objectName: "addrCombo"
                Layout.fillWidth: true
                model: addressModel
                textRole: "text"
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Currently connected via: %1").arg(hostSettings.activeAddress ? hostSettings.activeAddress : qsTr("(offline)"))
                font.pointSize: 9
                wrapMode: Text.Wrap
            }

            // ---- Stream settings profile ----
            CheckBox {
                id: customProfileCheck
                Layout.topMargin: 10
                text: qsTr("Use custom stream settings for this PC")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("When enabled, these settings replace the global settings for streams from this PC.")
                font.pointSize: 9
                wrapMode: Text.Wrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 10
                enabled: customProfileCheck.checked

                Label { text: qsTr("Resolution") }
                ComboBox {
                    id: resolutionComboBox
                    Layout.fillWidth: true
                    model: {
                        var items = []
                        for (var i = 0; i < resolutionPresets.length; i++) {
                            items.push(resolutionPresets[i].text)
                        }
                        items.push(qsTr("Custom"))
                        return items
                    }
                }

                Label {
                    text: qsTr("Custom size")
                    visible: resolutionComboBox.currentIndex === resolutionPresets.length
                }
                RowLayout {
                    visible: resolutionComboBox.currentIndex === resolutionPresets.length
                    TextField {
                        id: customWidthField
                        Layout.preferredWidth: 90
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 256; top: 8192 }
                    }
                    Label { text: "x" }
                    TextField {
                        id: customHeightField
                        Layout.preferredWidth: 90
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 256; top: 8192 }
                    }
                }

                Label { text: qsTr("Frame rate (FPS)") }
                SpinBox {
                    id: fpsSpinBox
                    from: 10
                    to: 480
                    editable: true
                }

                Label { text: qsTr("Bitrate") }
                ColumnLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        id: autoBitrateCheck
                        text: qsTr("Automatic (%1 Mbps for this resolution and FPS)")
                                .arg(StreamingPreferences.getDefaultBitrate(selectedWidth() || 0, selectedHeight() || 0,
                                                                            fpsSpinBox.value, StreamingPreferences.enableYUV444) / 1000.0)
                    }
                    RowLayout {
                        visible: !autoBitrateCheck.checked
                        Slider {
                            id: bitrateSlider
                            Layout.fillWidth: true
                            from: 500
                            to: StreamingPreferences.unlockBitrate ? 500000 : 150000
                            stepSize: 500
                        }
                        Label { text: qsTr("%1 Mbps").arg(bitrateSlider.value / 1000.0) }
                    }
                }

                Label { text: qsTr("Display mode") }
                ComboBox {
                    id: windowModeComboBox
                    Layout.fillWidth: true
                    model: windowModeModel
                    textRole: "text"
                }

                Label { text: qsTr("Controller type") }
                ComboBox {
                    id: controllerTypeComboBox
                    Layout.fillWidth: true
                    model: controllerTypeModel
                    textRole: "text"
                }

                Label { text: qsTr("Virtual display"); Layout.alignment: Qt.AlignTop; Layout.topMargin: 12 }
                ColumnLayout {
                    Layout.fillWidth: true
                    CheckBox { id: virtualDisplayCheck; text: qsTr("Use an Apollo virtual display") }
                    Label {
                        Layout.fillWidth: true
                        font.pointSize: 9
                        wrapMode: Text.Wrap
                        text: hostSettings.virtualDisplayStatus ? hostSettings.virtualDisplayStatus : ""
                        visible: text !== ""
                    }
                }

                Label { text: qsTr("Render scale") }
                ComboBox {
                    id: renderScaleComboBox
                    Layout.fillWidth: true
                    model: renderScaleModel
                    textRole: "text"
                }

                Label { text: qsTr("Options"); Layout.alignment: Qt.AlignTop; Layout.topMargin: 12 }
                ColumnLayout {
                    CheckBox { id: hdrCheck; text: qsTr("Enable HDR") }
                    CheckBox { id: mouseCheck; text: qsTr("Optimize mouse for remote desktop instead of games") }
                    CheckBox { id: perfOverlayCheck; text: qsTr("Show performance stats") }
                    CheckBox { id: sharpenCheck; text: qsTr("Sharpen video (AMD FSR RCAS)") }
                }

                Label { text: qsTr("Sharpness"); visible: sharpenCheck.checked }
                RowLayout {
                    visible: sharpenCheck.checked
                    Slider { id: sharpnessSlider; Layout.fillWidth: true; from: 5; to: 100; stepSize: 5 }
                    Label { text: sharpnessSlider.value + "%" }
                }

                Label { text: qsTr("Brightness") }
                RowLayout {
                    Slider { id: brightnessSlider; Layout.fillWidth: true; from: -30; to: 30; stepSize: 1 }
                    Label { text: (brightnessSlider.value > 0 ? "+" : "") + brightnessSlider.value + "%" }
                }

                Label { text: qsTr("Contrast") }
                RowLayout {
                    Slider { id: contrastSlider; Layout.fillWidth: true; from: 50; to: 150; stepSize: 1 }
                    Label { text: contrastSlider.value + "%" }
                }

                Label { text: qsTr("Saturation") }
                RowLayout {
                    Slider { id: saturationSlider; Layout.fillWidth: true; from: 0; to: 200; stepSize: 1 }
                    Label { text: saturationSlider.value + "%" }
                }
            }
        }
    }
}
