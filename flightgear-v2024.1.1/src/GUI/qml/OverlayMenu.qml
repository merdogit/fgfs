import QtQuick 2.4
import QtQuick.Controls 2.2
import FlightGear 1.0

Rectangle {
    id: root

    property var model: undefined
    property int currentIndex: 0
    property string headerText: ""
    property int maximumPermittedHeight: 450
    property string displayRole: "display"

    color: Style.panelBackground
    border.width: 1
    border.color: Style.minorFrameColor
    width: flick.width + Style.margin
    height: flick.height + Style.margin
    clip: true

    // Overlay control properties
    property bool dismissOnClickOutside: true
    property bool alignRightEdge: false

    signal select(var index);

    Component.onCompleted: {
        computeMenuWidth();
    }

    y: -OverlayShared.globalOverlay.verticalOverflowFor(height)
    x: alignRightEdge ? -width : 0

    function close()
    {
        OverlayShared.globalOverlay.dismissOverlay()
    }

    function doSelect(index)
    {
        select(index);
        close();
    }

    function computeMenuWidth()
    {
        var minWidth = 0;
        for (var i = 0; i < choicesRepeater.count; i++) {
            minWidth = Math.max(minWidth, choicesRepeater.itemAt(i).implicitWidth);
        }
        if (root.haveHeader())
            minWidth = Math.max(minWidth, header.width);
         flick.width = minWidth
    }

    function haveHeader()
    {
        return headerText !== "";
    }

    Flickable {
        id: flick
        anchors.centerIn: parent
        ScrollBar.vertical: ScrollBar {}
        height: Math.min(root.maximumPermittedHeight, contentHeight);
        contentHeight: itemsColumn.childrenRect.height

        Column {
            id: itemsColumn
            width: flick.width
            spacing: Style.margin

            PopupChoiceItem {
                id: header
                text: root.headerText
                visible: root.haveHeader()
                onSelect: root.doSelect(-1);
            }

            // main item repeater
            Repeater {
                id: choicesRepeater
                model: root.model
                delegate: PopupChoiceItem {
                    isCurrentIndex: root.currentIndex === model.index
                    onSelect: function select(index) {
                        root.doSelect(index)
                    }
                    
                    text: (Array.isArray(model) ? modelData[displayRole] : model[displayRole]) 
                    // text: model && model.hasOwnProperty(displayRole) ? model[displayRole] // Qml ListModel and QAbstractItemModel
                    //                                                  : modelData && modelData.hasOwnProperty(displayRole) ? modelData[displayRole] // QObjectList / QObject
                    //                                                                                                       : modelData != undefined ? modelData : "" // Models without role
                }
            } // menu item repeater
        } // of menu contents column
    }
}
