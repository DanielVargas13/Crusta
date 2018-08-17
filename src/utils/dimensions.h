#pragma once

#include <QMargins>

class Dimensions
{
    explicit Dimensions();
public:
    static int sideBarWidth();
    static int tabBarHeight();
    static int onmibarHeightOffsetFromParent();
    static QMargins omnibarMargins();
};
