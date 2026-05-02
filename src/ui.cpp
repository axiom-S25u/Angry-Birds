#include "ui.hpp"
#include "levels/levels.hpp"
#include <cmath>

// SHOP
static int dmgCost(int lvl) { return 100 * (int)powf(2.0f, (float)lvl); }
static int birdCost(int lvl) { return 500 * (int)powf(2.0f, (float)lvl); }

AngryShopLayer* AngryShopLayer::create() {
    AngryShopLayer* r = new AngryShopLayer();
    if (r && r->init()) { r->autorelease(); return r; }
    delete r; return nullptr;
}

bool AngryShopLayer::init() {
    if (!CCLayer::init()) return false;
    CCSize ws = CCDirector::sharedDirector()->getWinSize();

    CCLayerColor* bg = CCLayerColor::create(ccc4(30, 50, 70, 255));
    bg->setContentSize(ws);
    this->addChild(bg, topZ(this) + 1);

    CCLabelBMFont* title = CCLabelBMFont::create("SHOP", "bigFont.fnt");
    title->setPosition(ccp(ws.width / 2.0f, ws.height - 40.0f));
    this->addChild(title, topZ(this) + 1);

    coinLbl = CCLabelBMFont::create(fmt::format("Coins: {}", g_coins).c_str(), "bigFont.fnt");
    coinLbl->setPosition(ccp(ws.width / 2.0f, ws.height - 75.0f));
    coinLbl->setScale(0.4f);
    this->addChild(coinLbl, topZ(this) + 1);

    CCMenu* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    this->addChild(menu, topZ(this) + 1);

    float y1 = ws.height / 2.0f + 40.0f;
    float y2 = ws.height / 2.0f - 40.0f;

    // DAMAGE UPGRADE
    CCLabelBMFont* dmgTitle = CCLabelBMFont::create("DAMAGE", "bigFont.fnt");
    dmgTitle->setScale(0.7f);
    dmgTitle->setAnchorPoint(ccp(0.0f, 0.5f));
    dmgTitle->setPosition(ccp(80.0f, y1 + 18.0f));
    this->addChild(dmgTitle, topZ(this) + 1);

    dmgLbl = CCLabelBMFont::create("", "bigFont.fnt");
    dmgLbl->setScale(0.4f);
    dmgLbl->setAnchorPoint(ccp(0.0f, 0.5f));
    dmgLbl->setPosition(ccp(80.0f, y1 - 10.0f));
    this->addChild(dmgLbl, topZ(this) + 1);

    ButtonSprite* dmgSpr = ButtonSprite::create("BUY", 0, false, "bigFont.fnt", "GJ_button_01.png", 40.0f, 0.7f);
    dmgBtn = CCMenuItemSpriteExtra::create(dmgSpr, this, menu_selector(AngryShopLayer::onBuyDmg));
    dmgBtn->setPosition(ccp(ws.width - 80.0f, y1));
    menu->addChild(dmgBtn, topZ(menu) + 1);

    // BIRD UPGRADEEEEEEEEEEEEEEEEEE
    CCLabelBMFont* birdTitle = CCLabelBMFont::create("BONUS BIRDS", "bigFont.fnt");
    birdTitle->setScale(0.7f);
    birdTitle->setAnchorPoint(ccp(0.0f, 0.5f));
    birdTitle->setPosition(ccp(80.0f, y2 + 18.0f));
    this->addChild(birdTitle, topZ(this) + 1);

    birdLbl = CCLabelBMFont::create("", "bigFont.fnt");
    birdLbl->setScale(0.4f);
    birdLbl->setAnchorPoint(ccp(0.0f, 0.5f));
    birdLbl->setPosition(ccp(80.0f, y2 - 10.0f));
    this->addChild(birdLbl, topZ(this) + 1);

    ButtonSprite* birdSpr = ButtonSprite::create("BUY", 0, false, "bigFont.fnt", "GJ_button_01.png", 40.0f, 0.7f);
    birdBtn = CCMenuItemSpriteExtra::create(birdSpr, this, menu_selector(AngryShopLayer::onBuyBird));
    birdBtn->setPosition(ccp(ws.width - 80.0f, y2));
    menu->addChild(birdBtn, topZ(menu) + 1);

    // close button
    CCSprite* closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    CCMenuItemSpriteExtra* closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(AngryShopLayer::onClose));
    closeBtn->setPosition(ccp(25.0f, ws.height - 25.0f));
    menu->addChild(closeBtn, topZ(menu) + 1);

    refreshLabels();
    this->setKeypadEnabled(true);
    return true;
}

void AngryShopLayer::keyBackClicked() { onClose(nullptr); }

void AngryShopLayer::refreshLabels() {
    coinLbl->setString(fmt::format("Coins: {}", g_coins).c_str());

    if (g_dmgUp >= 5) {
        dmgLbl->setString(fmt::format("Lv {}/5 (+{}% dmg) MAXED", g_dmgUp, (int)(g_dmgUp * 15)).c_str());
        dmgBtn->setEnabled(false);
    } else {
        int cost = dmgCost(g_dmgUp);
        dmgLbl->setString(fmt::format("Lv {}/5 (+{}% dmg)  Cost: {}", g_dmgUp, (int)(g_dmgUp * 15), cost).c_str());
        dmgBtn->setEnabled(g_coins >= cost);
    }

    if (g_birdUp >= 3) {
        birdLbl->setString(fmt::format("Lv {}/3 (+{} birds) MAXED", g_birdUp, g_birdUp).c_str());
        birdBtn->setEnabled(false);
    } else {
        int cost = birdCost(g_birdUp);
        birdLbl->setString(fmt::format("Lv {}/3 (+{} birds)  Cost: {}", g_birdUp, g_birdUp, cost).c_str());
        birdBtn->setEnabled(g_coins >= cost);
    }
}

void AngryShopLayer::onBuyDmg(CCObject*) {
    if (g_dmgUp >= 5) return;
    int cost = dmgCost(g_dmgUp);
    if (g_coins < cost) return;
    g_coins -= cost;
    g_dmgUp++;
    Mod::get()->setSavedValue<int>("coins", g_coins);
    Mod::get()->setSavedValue<int>("dmg_up", g_dmgUp);
    refreshLabels();
}

void AngryShopLayer::onBuyBird(CCObject*) {
    if (g_birdUp >= 3) return;
    int cost = birdCost(g_birdUp);
    if (g_coins < cost) return;
    g_coins -= cost;
    g_birdUp++;
    Mod::get()->setSavedValue<int>("coins", g_coins);
    Mod::get()->setSavedValue<int>("bird_up", g_birdUp);
    refreshLabels();
}

void AngryShopLayer::onClose(CCObject*) {
    CCDirector::sharedDirector()->popScene();
}

// LEVEL SELECT
AngryLevelSelectLayer* AngryLevelSelectLayer::create() {
    AngryLevelSelectLayer* r = new AngryLevelSelectLayer();
    if (r && r->init()) { r->autorelease(); return r; }
    delete r; return nullptr;
}

bool AngryLevelSelectLayer::init() {
    if (!CCLayer::init()) return false;
    CCSize ws = CCDirector::sharedDirector()->getWinSize();

    CCLayerColor* bg = CCLayerColor::create(ccc4(40, 60, 90, 255));
    bg->setContentSize(ws);
    this->addChild(bg, topZ(this) + 1);

    CCLabelBMFont* title = CCLabelBMFont::create("PICK YOUR LEVEL", "bigFont.fnt");
    title->setPosition(ccp(ws.width / 2.0f, ws.height - 35.0f));
    title->setScale(0.7f);
    this->addChild(title, topZ(this) + 1);

    CCLabelBMFont* hi = CCLabelBMFont::create(fmt::format("Hi-Score: {}", g_high).c_str(), "bigFont.fnt");
    hi->setScale(0.35f);
    hi->setAnchorPoint(ccp(0.0f, 0.5f));
    hi->setPosition(ccp(70.0f, ws.height - 55.0f));
    this->addChild(hi, topZ(this) + 1);

    coinLbl = CCLabelBMFont::create(fmt::format("Coins: {}", g_coins).c_str(), "bigFont.fnt");
    coinLbl->setScale(0.35f);
    coinLbl->setAnchorPoint(ccp(1.0f, 0.5f));
    coinLbl->setPosition(ccp(ws.width - 70.0f, ws.height - 55.0f));
    this->addChild(coinLbl, topZ(this) + 1);

    // scrollable content node holds all level buttons + arrows
    scrollContent = CCNode::create();
    scrollContent->setPosition(ccp(0.0f, 0.0f));
    scrollContent->setContentSize(ws);
    this->addChild(scrollContent, topZ(this) + 1);

    buildLevelGrid(ws);

    // close button
    CCMenu* topMenu = CCMenu::create();
    topMenu->setPosition(ccp(0.0f, 0.0f));
    CCSprite* closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    CCMenuItemSpriteExtra* closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(AngryLevelSelectLayer::onClose));
    closeBtn->setPosition(ccp(25.0f, ws.height - 25.0f));
    topMenu->addChild(closeBtn, topZ(topMenu) + 1);

    // SHOP button
    ButtonSprite* shopSpr = ButtonSprite::create("SHOP", 0, false, "bigFont.fnt", "GJ_button_04.png", 50.0f, 0.7f);
    CCMenuItemSpriteExtra* shopBtn = CCMenuItemSpriteExtra::create(shopSpr, this, menu_selector(AngryLevelSelectLayer::onShop));
    shopBtn->setPosition(ccp(ws.width - 50.0f, 30.0f));
    topMenu->addChild(shopBtn, topZ(topMenu) + 1);

    this->addChild(topMenu, topZ(this) + 1);

    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);
    return true;
}

void AngryLevelSelectLayer::buildLevelGrid(CCSize ws) {
    std::vector<LevelDef> levels = getLevels();
    int n = (int)levels.size();
    // 5 levels per row, snake pattern
    int perRow = 5;
    float cellW = 95.0f;
    float rowH = 110.0f;
    float topY = ws.height - 130.0f;  // start below title/coins
    float leftX = ws.width / 2.0f - (perRow - 1) * cellW / 2.0f;

    CCMenu* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    scrollContent->addChild(menu, topZ(scrollContent) + 1);

    CCDrawNode* arrows = CCDrawNode::create();
    scrollContent->addChild(arrows, topZ(scrollContent) + 1);

    for (int i = 0; i < n; i++) {
        int row = i / perRow;
        int col = i % perRow;
        // snake: even rows go right, odd rows go left
        float x = (row % 2 == 0)
            ? leftX + col * cellW
            : leftX + (perRow - 1 - col) * cellW;
        float y = topY - row * rowH;

        bool unlocked = (i + 1) <= g_unlocked;
        ButtonSprite* spr;
        if (unlocked) {
            spr = ButtonSprite::create(fmt::format("{}", i + 1).c_str(), 50, false, "bigFont.fnt", "GJ_button_01.png", 55.0f, 1.0f);
        } else {
            spr = ButtonSprite::create("?", 50, false, "bigFont.fnt", "GJ_button_05.png", 55.0f, 1.0f);
            spr->setColor(ccc3(120, 120, 120));
        }
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AngryLevelSelectLayer::onLevel));
        btn->setTag(i);
        btn->setPosition(ccp(x, y));
        if (!unlocked) btn->setEnabled(false);
        menu->addChild(btn, topZ(menu) + 1);

        CCLabelBMFont* nm = CCLabelBMFont::create(unlocked ? levels[i].name : "LOCKED", "bigFont.fnt");
        nm->setScale(0.35f);
        nm->setPosition(ccp(x, y - 40.0f));
        scrollContent->addChild(nm, topZ(scrollContent) + 1);

        // draw arrow from previous level to this one
        if (i > 0) {
            int prevRow = (i - 1) / perRow;
            int prevCol = (i - 1) % perRow;
            float px = (prevRow % 2 == 0)
                ? leftX + prevCol * cellW
                : leftX + (perRow - 1 - prevCol) * cellW;
            float py = topY - prevRow * rowH;

            ccColor4F arrowCol = unlocked ? ccc4f(1.0f, 1.0f, 1.0f, 0.8f) : ccc4f(0.5f, 0.5f, 0.5f, 0.5f);

            if (row == prevRow) {
                // horizontal arrow
                float x1 = fminf(px, x) + 28.0f;
                float x2 = fmaxf(px, x) - 28.0f;
                arrows->drawSegment(ccp(x1, y), ccp(x2, y), 2.0f, arrowCol);
                // arrowhead pointing toward current (x)
                float headX = x + (x < px ? 28.0f : -28.0f);
                float dir = (x < px) ? 1.0f : -1.0f;
                arrows->drawSegment(ccp(headX, y), ccp(headX + dir * 8.0f, y + 6.0f), 2.0f, arrowCol);
                arrows->drawSegment(ccp(headX, y), ccp(headX + dir * 8.0f, y - 6.0f), 2.0f, arrowCol);
            } else {
                // vertical arrow (going down to new row)
                float y1 = py - 32.0f;
                float y2 = y + 32.0f;
                arrows->drawSegment(ccp(px, y1), ccp(px, y2), 2.0f, arrowCol);
                arrows->drawSegment(ccp(px, y2), ccp(px - 6.0f, y2 + 8.0f), 2.0f, arrowCol);
                arrows->drawSegment(ccp(px, y2), ccp(px + 6.0f, y2 + 8.0f), 2.0f, arrowCol);
            }
        }
    }

    // scroll range: if content fits, no scroll. else allow scrolling.
    int rows = (n + perRow - 1) / perRow;
    float totalH = rows * rowH;
    float visibleH = ws.height - 180.0f; // minus top ui
    scrollY = 0.0f;
    scrollMin = 0.0f;
    scrollMax = fmaxf(0.0f, totalH - visibleH);
}

void AngryLevelSelectLayer::registerWithTouchDispatcher() {
    CCTouchDispatcher::get()->addTargetedDelegate(this, -100, true);
}

void AngryLevelSelectLayer::onExit() {
    CCTouchDispatcher::get()->removeDelegate(this);
    CCLayer::onExit();
}

bool AngryLevelSelectLayer::ccTouchBegan(CCTouch* t, CCEvent*) {
    lastTouch = t->getLocation();
    scrolling = false;
    return true;
}

void AngryLevelSelectLayer::ccTouchMoved(CCTouch* t, CCEvent*) {
    if (scrollMax <= 0.0f) return;
    CCPoint p = t->getLocation();
    float dy = p.y - lastTouch.y;
    if (fabsf(dy) > 3.0f) scrolling = true;
    scrollY += dy;
    if (scrollY < scrollMin) scrollY = scrollMin;
    if (scrollY > scrollMax) scrollY = scrollMax;
    scrollContent->setPositionY(scrollY);
    lastTouch = p;
}

void AngryLevelSelectLayer::ccTouchEnded(CCTouch*, CCEvent*) { scrolling = false; }

void AngryLevelSelectLayer::keyBackClicked() { onClose(nullptr); }

void AngryLevelSelectLayer::onLevel(CCObject* sender) {
    if (scrolling) return;
    int idx = ((CCNode*)sender)->getTag();
    CCScene* sc = CCScene::create();
    sc->addChild(makeAngryLayer(idx));
    CCDirector::sharedDirector()->pushScene(sc);
}

void AngryLevelSelectLayer::onShop(CCObject*) {
    CCScene* sc = CCScene::create();
    sc->addChild(AngryShopLayer::create());
    CCDirector::sharedDirector()->pushScene(sc);
}

void AngryLevelSelectLayer::onClose(CCObject*) {
    g_open = false;
    CCDirector::sharedDirector()->popScene();
}

void AngryLevelSelectLayer::onEnter() {
    CCLayer::onEnter();
    if (!didEnter) { didEnter = true; return; }
    this->removeAllChildrenWithCleanup(true);
    this->init();
}
