#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// globals defined in main.cpp
extern bool g_open;
extern int g_high;
extern int g_unlocked;
extern int g_coins;
extern int g_dmgUp;
extern int g_birdUp;

int topZ(CCNode* p);

// lives in main.cpp, cant include AngryLayer here without chaos
CCLayer* makeAngryLayer(int idx);

class AngryShopLayer : public CCLayer {
public:
    CCLabelBMFont* coinLbl;
    CCLabelBMFont* dmgLbl;
    CCLabelBMFont* birdLbl;
    CCMenuItemSpriteExtra* dmgBtn;
    CCMenuItemSpriteExtra* birdBtn;

    static AngryShopLayer* create();
    bool init() override;
    void keyBackClicked() override;
    void refreshLabels();
    void onBuyDmg(CCObject*);
    void onBuyBird(CCObject*);
    void onClose(CCObject*);
};

class AngryLevelSelectLayer : public CCLayer {
public:
    CCNode* scrollContent;
    CCLabelBMFont* coinLbl;
    float scrollY;
    float scrollMin;
    float scrollMax;
    CCPoint lastTouch;
    bool scrolling = false;
    bool didEnter = false;

    static AngryLevelSelectLayer* create();
    bool init() override;
    void buildLevelGrid(CCSize ws);
    void registerWithTouchDispatcher() override;
    void onExit() override;
    bool ccTouchBegan(CCTouch* t, CCEvent*) override;
    void ccTouchMoved(CCTouch* t, CCEvent*) override;
    void ccTouchEnded(CCTouch*, CCEvent*) override;
    void keyBackClicked() override;
    void onLevel(CCObject* sender);
    void onShop(CCObject*);
    void onClose(CCObject*);
    void onEnter() override;
};
