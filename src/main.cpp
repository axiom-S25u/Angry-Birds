#include <Geode/Geode.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <cmath>
#include <vector>
#include <cstdlib>

#include <box2d/World.h>
#include <box2d/Body.h>

#include "levels/levels.hpp"
#include "ui.hpp"

using namespace geode::prelude;

int topZ(CCNode* p) {
    int m = 0;
    if (p) {
        for (CCNode* n : p->getChildrenExt()) if (n->getZOrder() > m) m = n->getZOrder();
    }
    return m;
}

bool g_open = false;
int g_high = 0;
int g_unlocked = 1; // how many levels unlocked
int g_coins = 0;    // shop currency
int g_dmgUp = 0;    // damage upgrade level (0-5)
int g_birdUp = 0;   // bonus birds upgrade level (0-3)

static float getDamageMult() { return 1.0f + 0.15f * (float)g_dmgUp; }
static int getBirdBonus() { return g_birdUp; }

struct Pig {
    SimplePlayer* spr;
    Body* body;
    float hp;
    bool dead;
};

struct Brick {
    CCLayerColor* shit;
    CCLabelBMFont* hpLbl;
    Body* body;
    float hp;
    bool dead;
};

class AngryLayer : public CCLayer {
public:
    SimplePlayer* slingBird;
    SimplePlayer* flyBird;
    Body* flyBody;
    CCPoint slingPos;
    CCPoint birdPos;
    CCPoint birdVel;
    bool flying;
    bool aiming;
    CCPoint dragP;
    float floorY;
    float wallR;
    float restTimer;
    float birdLifetime;
    int score;
    int birdsLeft;
    bool ended;
    int currentIcon;
    int currentColor;
    CCDrawNode* drawer;
    World* world;
    std::vector<Pig> pigs;
    std::vector<Brick> bricks;
    CCLabelBMFont* scoreLbl;
    CCLabelBMFont* msgLbl;
    CCLabelBMFont* birdsLbl;

    int levelIdx;

    static AngryLayer* create(int lvl) {
        AngryLayer* r = new AngryLayer();
        r->levelIdx = lvl;
        if (r && r->init()) { r->autorelease(); return r; }
        delete r; return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto levels = getLevels();
        if (levelIdx < 0 || levelIdx >= (int)levels.size()) levelIdx = 0;
        LevelDef& lvl = levels[levelIdx];
        CCSize ws = CCDirector::sharedDirector()->getWinSize();
        slingBird = nullptr;
        flyBird = nullptr;
        flyBody = nullptr;
        flying = false;
        aiming = false;
        floorY = 60.0f;
        wallR = ws.width - 20.0f;
        restTimer = 0.0f;
        birdLifetime = 0.0f;
        score = 0;
        birdsLeft = lvl.birds + getBirdBonus();
        ended = false;
        currentIcon = 1;
        currentColor = 0;
        slingPos = ccp(140.0f, floorY + 70.0f);
        drawer = nullptr;
        FMODAudioEngine::sharedEngine()->stopAllMusic(true);
        // theme song loop, prob gonna get muted in 5s
        FMODAudioEngine::sharedEngine()->playMusic("themesong.mp3"_spr, true, 0.0f, 0);
        // force play again in case it didnt start first time (android hates me)
        this->scheduleOnce(schedule_selector(AngryLayer::retryMusic), 0.1f);
        
        // box2d world setup
        world = new World(Vec2(0.0f, -900.0f), 16);
        World::accumulateImpulses = true;
        World::warmStarting = true;
        World::positionCorrection = true;

        CCLayerColor* sky = CCLayerColor::create(ccc4(135, 206, 235, 255));
        sky->setContentSize(ws);
        this->addChild(sky, topZ(this) + 1);

        for (int i = 0; i < 5; i++) {
            CCLayerColor* cloud = CCLayerColor::create(ccc4(255, 255, 255, 200), 80.0f, 22.0f);
            cloud->setPosition(ccp((float)(40 + i * 130 + rand() % 50), ws.height - 80.0f - (float)(rand() % 60)));
            this->addChild(cloud, topZ(this) + 1);
        }

        CCLayerColor* ground = CCLayerColor::create(ccc4(120, 70, 30, 255), ws.width, floorY);
        this->addChild(ground, topZ(this) + 1);
        CCLayerColor* grass = CCLayerColor::create(ccc4(90, 180, 60, 255), ws.width, 8.0f);
        grass->setPosition(ccp(0.0f, floorY - 4.0f));
        this->addChild(grass, topZ(this) + 1);

        CCLayerColor* postL = CCLayerColor::create(ccc4(80, 40, 10, 255), 8.0f, 60.0f);
        postL->setPosition(ccp(slingPos.x - 14.0f, floorY));
        postL->setRotation(15.0f);
        this->addChild(postL, topZ(this) + 1);
        CCLayerColor* postR = CCLayerColor::create(ccc4(80, 40, 10, 255), 8.0f, 60.0f);
        postR->setPosition(ccp(slingPos.x + 6.0f, floorY));
        postR->setRotation(-15.0f);
        this->addChild(postR, topZ(this) + 1);
        CCLayerColor* postBase = CCLayerColor::create(ccc4(60, 30, 10, 255), 18.0f, 12.0f);
        postBase->setPosition(ccp(slingPos.x - 9.0f, floorY));
        this->addChild(postBase, topZ(this) + 1);

        drawer = CCDrawNode::create();
        this->addChild(drawer, topZ(this) + 1);

        buildLevel();
        spawnSlingBird();
        setupHud();
        setupButtons();

        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);
        this->schedule(schedule_selector(AngryLayer::tick), 1.0f / 60.0f);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -128, true);
    }

    void onExit() override {
        CCTouchDispatcher::get()->removeDelegate(this);
        FMODAudioEngine::sharedEngine()->stopAllMusic(true);
        FMODAudioEngine::sharedEngine()->stopAllEffects();
        CCLayer::onExit();
    }

    void keyBackClicked() override { onClose(nullptr); }

    void buildLevel() {
        CCSize ws = CCDirector::sharedDirector()->getWinSize();
        float baseX = ws.width - 240.0f;
        auto levels = getLevels();
        LevelDef& lvl = levels[levelIdx];
        for (BrickDef& b : lvl.bricks) {
            addBrick(baseX + b.x, floorY + b.y, b.w, b.h, b.hp);
        }
        for (PigDef& p : lvl.pigs) {
            addPig(baseX + p.x, floorY + p.y);
        }
        // pre-settle stacked shit so towers dont topple on their own
        for (int i = 0; i < 60; i++) {
            world->Step(1.0f / 60.0f);
            for (Brick& br : bricks) {
                if (!br.body) continue;
                // clamp to floor + kill micro jitter
                if (br.body->position.y - br.body->width.y/2.0f < floorY) {
                    br.body->position.y = floorY + br.body->width.y/2.0f;
                    br.body->velocity.y = 0.0f;
                }
                br.body->velocity.x *= 0.3f;
                if (fabsf(br.body->velocity.x) < 3.0f) br.body->velocity.x = 0.0f;
                br.body->velocity.y *= 0.3f;
                br.body->angularVelocity *= 0.3f;
            }
            for (Pig& pg : pigs) {
                if (!pg.body) continue;
                pg.body->velocity.x *= 0.3f;
                pg.body->velocity.y *= 0.3f;
                pg.body->angularVelocity *= 0.3f;
            }
        }
        // zero out all leftover motion after settling
        for (Brick& br : bricks) {
            if (!br.body) continue;
            br.body->velocity = Vec2(0.0f, 0.0f);
            br.body->angularVelocity = 0.0f;
        }
        for (Pig& pg : pigs) {
            if (!pg.body) continue;
            pg.body->velocity = Vec2(0.0f, 0.0f);
            pg.body->angularVelocity = 0.0f;
        }
    }

    void addBrick(float x, float y, float w, float h, float hp) {
        Brick br;
        br.hp = hp;
        br.dead = false;
        
        // box2d body
        Body* body = new Body();
        Vec2 brickSize(w, h);
        float mass = 2.0f + (w * h) * 0.0008f;
        body->Set(brickSize, mass);
        body->position = Vec2(x + w/2.0f, y + h/2.0f);
        body->friction = 0.9f;
        world->Add(body);
        br.body = body;
        
        // cocos visual
        br.shit = CCLayerColor::create(ccc4(160, 100, 40, 255), w, h);
        br.shit->setPosition(ccp(x, y));
        this->addChild(br.shit, topZ(this) + 1);
        CCLayerColor* topStripe = CCLayerColor::create(ccc4(80, 40, 15, 255), w, 3.0f);
        topStripe->setPosition(ccp(0.0f, h - 3.0f));
        br.shit->addChild(topStripe);
        br.hpLbl = CCLabelBMFont::create(fmt::format("{}", (int)hp).c_str(), "bigFont.fnt");
        br.hpLbl->setScale(0.35f);
        br.hpLbl->setPosition(ccp(x + w/2.0f, y + h/2.0f));
        this->addChild(br.hpLbl, topZ(this) + 1);
        
        bricks.push_back(br);
    }

    void addPig(float x, float y) {
        Pig pg;
        pg.hp = 60.0f;
        pg.dead = false;
        
        // box2d body
        Body* body = new Body();
        Vec2 pigSize(32.0f, 32.0f);
        body->Set(pigSize, 1.0f);
        body->position = Vec2(x, y);
        body->friction = 0.3f;
        world->Add(body);
        pg.body = body;
        
        // cocos visual
        pg.spr = SimplePlayer::create(0);
        pg.spr->updatePlayerFrame(1 + rand() % 30, IconType::Cube);
        pg.spr->setColor(ccc3(80, 220, 80));
        pg.spr->setSecondColor(ccc3(40, 140, 40));
        pg.spr->updateColors();
        pg.spr->setScale(0.85f);
        pg.spr->setPosition(ccp(x, y));
        this->addChild(pg.spr, topZ(this) + 1);
        
        pigs.push_back(pg);
    }

    void spawnSlingBird() {
        if (ended) return;
        if (birdsLeft <= 0) { checkWin(); return; }
        currentIcon = 1 + rand() % 30;
        currentColor = rand() % 12;
        GameManager* gm = GameManager::sharedState();
        slingBird = SimplePlayer::create(0);
        int t = rand() % 3;
        IconType it = IconType::Cube;
        if (t == 1) it = IconType::Ball;
        if (t == 2) it = IconType::Ufo;
        slingBird->updatePlayerFrame(currentIcon, it);
        slingBird->setColor(gm->colorForIdx(currentColor));
        slingBird->setSecondColor(gm->colorForIdx((currentColor + 4) % 12));
        slingBird->updateColors();
        slingBird->setScale(0.95f);
        slingBird->setPosition(slingPos);
        this->addChild(slingBird, topZ(this) + 1);
        if (birdsLbl) birdsLbl->setString(fmt::format("Birds: {}", birdsLeft).c_str());
    }

    void setupHud() {
        CCSize ws = CCDirector::sharedDirector()->getWinSize();
        scoreLbl = CCLabelBMFont::create(fmt::format("Score: 0  Hi: {}", g_high).c_str(), "bigFont.fnt");
        scoreLbl->setScale(0.5f);
        scoreLbl->setPosition(ccp(ws.width / 2.0f, ws.height - 20.0f));
        this->addChild(scoreLbl, topZ(this) + 1);

        msgLbl = CCLabelBMFont::create("", "bigFont.fnt");
        msgLbl->setPosition(ccp(ws.width / 2.0f, ws.height / 2.0f + 40.0f));
        msgLbl->setScale(0.9f);
        this->addChild(msgLbl, topZ(this) + 1);

        birdsLbl = CCLabelBMFont::create(fmt::format("Birds: {}", birdsLeft).c_str(), "bigFont.fnt");
        birdsLbl->setScale(0.5f);
        birdsLbl->setAnchorPoint(ccp(0.0f, 0.5f));
        birdsLbl->setPosition(ccp(60.0f, ws.height - 20.0f));
        this->addChild(birdsLbl, topZ(this) + 1);
    }

    void setupButtons() {
        CCSize ws = CCDirector::sharedDirector()->getWinSize();
        CCMenu* m = CCMenu::create();
        m->setPosition(ccp(0.0f, 0.0f));
        CCSprite* closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        CCMenuItemSpriteExtra* closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(AngryLayer::onClose));
        closeBtn->setPosition(ccp(25.0f, ws.height - 25.0f));
        m->addChild(closeBtn, topZ(m) + 1);
        ButtonSprite* rspr = ButtonSprite::create("RESET", 0, false, "bigFont.fnt", "GJ_button_06.png", 30.0f, 0.5f);
        CCMenuItemSpriteExtra* rb = CCMenuItemSpriteExtra::create(rspr, this, menu_selector(AngryLayer::onReset));
        rb->setPosition(ccp(ws.width - 50.0f, ws.height - 25.0f));
        m->addChild(rb, topZ(m) + 1);
        this->addChild(m, topZ(this) + 1);
    }

    void onClose(CCObject*) {
        CCDirector::sharedDirector()->popScene();
    }

    void onReset(CCObject*) {
        CCScene* sc = CCScene::create();
        sc->addChild(AngryLayer::create(levelIdx));
        CCDirector::sharedDirector()->replaceScene(sc);
    }

    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (ended || flying || !slingBird) return true;
        CCPoint p = t->getLocation();
        if (ccpDistance(p, slingPos) < 90.0f) {
            aiming = true;
            dragP = p;
            return true;
        }
        return true;
    }

    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        if (!aiming) return;
        CCPoint p = t->getLocation();
        CCPoint diff = ccpSub(p, slingPos);
        float d = ccpLength(diff);
        float maxR = 90.0f;
        if (d > maxR) {
            diff = ccpMult(ccpNormalize(diff), maxR);
            p = ccpAdd(slingPos, diff);
        }
        dragP = p;
        if (slingBird) slingBird->setPosition(p);
        drawAim();
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (!aiming) return;
        aiming = false;
        if (drawer) drawer->clear();
        if (!slingBird) return;
        CCPoint launchVec = ccpMult(ccpSub(slingPos, dragP), 14.0f);
        if (ccpLength(launchVec) < 80.0f) {
            slingBird->setPosition(slingPos);
            return;
        }
        flying = true;
        flyBird = slingBird;
        slingBird = nullptr;
        birdPos = flyBird->getPosition();
        birdVel = launchVec;
        // throw sound roulette
        FMODAudioEngine::sharedEngine()->playEffect((rand() % 2 == 0 ? "throw.mp3"_spr : "throw2.mp3"_spr));
        
        // create box2d body for flying bird, its not a plane, not a rocket, its a BIRDDDDDDDDDDD or something idk i forgot
        flyBody = new Body();
        flyBody->Set(Vec2(32.0f, 32.0f), 1.0f);
        flyBody->position = Vec2(birdPos.x, birdPos.y);
        flyBody->velocity = Vec2(birdVel.x, birdVel.y);
        flyBody->friction = 0.3f;
        world->Add(flyBody);
        
        birdsLeft--;
        if (birdsLbl) birdsLbl->setString(fmt::format("Birds: {}", birdsLeft).c_str());
        restTimer = 0.0f;
        birdLifetime = 0.0f;
    }
// im forcing myself to comment more so its actually readable :O
    void retryMusic(float) {
        FMODAudioEngine::sharedEngine()->playMusic("themesong.mp3"_spr, true, 0.0f, 0);
    }

    void drawAim() {
        if (!drawer) return;
        drawer->clear();
        ccColor4F bandCol = ccc4f(0.3f, 0.18f, 0.06f, 1.0f);
        drawer->drawSegment(ccp(slingPos.x - 14.0f, slingPos.y - 5.0f), dragP, 2.5f, bandCol);
        drawer->drawSegment(ccp(slingPos.x + 6.0f, slingPos.y - 5.0f), dragP, 2.5f, bandCol);
        ccColor4F dotCol = ccc4f(1.0f, 1.0f, 1.0f, 0.7f);
        CCPoint vel = ccpMult(ccpSub(slingPos, dragP), 14.0f);
        CCPoint sim = dragP;
        float dt = 0.05f;
        for (int i = 0; i < 16; i++) {
            sim = ccpAdd(sim, ccpMult(vel, dt));
            vel.y -= 850.0f * dt;
            drawer->drawDot(sim, 2.5f, dotCol);
        }
    }

    void tick(float dt) {
        if (dt > 1.0f / 30.0f) dt = 1.0f / 30.0f;
        
        // step box2d
        world->Step(dt);
        
        // sync brick positions from box2d
        for (Brick& br : bricks) {
            if (br.dead || !br.body) continue;
            Vec2 pos = br.body->position;
            float rot = br.body->rotation;
            Vec2 sz = br.body->width;
            
            if (br.shit) {
                br.shit->setPosition(ccp(pos.x - sz.x/2.0f, pos.y - sz.y/2.0f));
                br.shit->setRotation(-rot * 180.0f / 3.14159f);
            }
            if (br.hpLbl) br.hpLbl->setPosition(ccp(pos.x, pos.y));
            
            // dampen angular velocity so shit doesnt spin forever
            br.body->angularVelocity *= 0.95f;
            if (fabsf(br.body->angularVelocity) < 0.05f) br.body->angularVelocity = 0.0f;
            // linear damping + velocity clamp so stacked bricks dont jitter themselves to death
            br.body->velocity.x *= 0.98f;
            if (fabsf(br.body->velocity.x) < 3.0f) br.body->velocity.x = 0.0f;
            if (fabsf(br.body->velocity.y) < 3.0f && pos.y - sz.y/2.0f < floorY + 1.0f) br.body->velocity.y = 0.0f;
            
            // floor collision (box2d doesn't have boundaries, we handle manually)
            if (pos.y - sz.y/2.0f < floorY) {
                float impact = fabsf(br.body->velocity.y);
                br.body->position.y = floorY + sz.y/2.0f;
                if (br.body->velocity.y < 0.0f) br.body->velocity.y = -br.body->velocity.y * 0.15f;
                br.body->velocity.x *= 0.82f;
                br.body->angularVelocity *= 0.7f;
                if (impact > 400.0f) {
                    br.hp -= impact * 0.03f;
                    if (br.hpLbl) br.hpLbl->setString(fmt::format("{}", (int)(br.hp < 0 ? 0 : br.hp)).c_str());
                    if (br.hp <= 0.0f) killBrick(br);
                }
            }
            if (!br.body) continue;
            // wall collision
            if (pos.x - sz.x/2.0f < 18.0f) {
                br.body->position.x = 18.0f + sz.x/2.0f;
                if (br.body->velocity.x < 0.0f) br.body->velocity.x = -br.body->velocity.x * 0.3f;
            }
            if (pos.x + sz.x/2.0f > wallR) {
                br.body->position.x = wallR - sz.x/2.0f;
                if (br.body->velocity.x > 0.0f) br.body->velocity.x = -br.body->velocity.x * 0.3f;
            }
        }
        
        // sync pig positions
        for (Pig& pg : pigs) {
            if (pg.dead || !pg.body) continue;
            Vec2 pos = pg.body->position;
            Vec2 sz = pg.body->width;
            
            if (pg.spr) {
                pg.spr->setPosition(ccp(pos.x, pos.y));
                pg.spr->setRotation(-pg.body->rotation * 180.0f / 3.14159f);
            }
            
            // angular damping
            pg.body->angularVelocity *= 0.93f;
            if (fabsf(pg.body->angularVelocity) < 0.05f) pg.body->angularVelocity = 0.0f;
            
            // floor collision
            if (pos.y - sz.y/2.0f < floorY) {
                float impact = fabsf(pg.body->velocity.y);
                pg.body->position.y = floorY + sz.y/2.0f;
                if (pg.body->velocity.y < 0.0f) pg.body->velocity.y = -pg.body->velocity.y * 0.2f;
                pg.body->velocity.x *= 0.78f;
                pg.body->angularVelocity *= 0.6f;
                if (impact > 300.0f) {
                    pg.hp -= impact * 0.1f;
                    if (pg.hp <= 0.0f) killPig(pg);
                }
            }
            if (!pg.body) continue;
            // wall collision
            if (pos.x - sz.x/2.0f < 18.0f) {
                pg.body->position.x = 18.0f + sz.x/2.0f;
                if (pg.body->velocity.x < 0.0f) pg.body->velocity.x = -pg.body->velocity.x * 0.3f;
            }
            if (pos.x + sz.x/2.0f > wallR) {
                pg.body->position.x = wallR - sz.x/2.0f;
                if (pg.body->velocity.x > 0.0f) pg.body->velocity.x = -pg.body->velocity.x * 0.3f;
            }
        }
        
        // bird collision damage
        if (flying && flyBody) {
            Vec2 birdPos = flyBody->position;
            Vec2 birdSz = flyBody->width;
            
            for (Brick& br : bricks) {
                if (br.dead || !br.body) continue;
                Vec2 brickPos = br.body->position;
                Vec2 brickSz = br.body->width;
                
                float dx = fabsf(birdPos.x - brickPos.x);
                float dy = fabsf(birdPos.y - brickPos.y);
                if (dx < (birdSz.x/2.0f + brickSz.x/2.0f) && dy < (birdSz.y/2.0f + brickSz.y/2.0f)) {
                    Vec2 vBefore = flyBody->velocity;
                    float speedBefore = vBefore.Length();
                    bool isResting = (brickPos.y - brickSz.y/2.0f < floorY + 2.0f) && fabsf(br.body->velocity.x) < 30.0f && fabsf(br.body->velocity.y) < 30.0f;
                    
                    float dmg = isResting ? (speedBefore * 0.05f + 3.0f) : (speedBefore * 0.18f + 10.0f);
                    dmg *= getDamageMult();
                    if (dmg > 400.0f) dmg = 400.0f;
                    br.hp -= dmg;
                    if (br.hpLbl) br.hpLbl->setString(fmt::format("{}", (int)(br.hp < 0 ? 0 : br.hp)).c_str());
                    FMODAudioEngine::sharedEngine()->playEffect("hit.mp3"_spr);
                    
                    // momentum transfer based on brick mass
                    float massRatio = br.body->mass / (br.body->mass + 1.0f);
                    Vec2 transfer = massRatio * 0.7f * vBefore;
                    br.body->velocity += br.body->invMass * transfer;
                    
                    // bird keeps velocity proportional to how much got transferred
                    // if brick dies, bird barely loses any velocity (demolish mode)
                    bool willDie = br.hp <= 0.0f;
                    float keepFactor = willDie ? 0.85f : (1.0f - massRatio * 0.4f);
                    flyBody->velocity *= keepFactor;
                    
                    // push bird out of brick to avoid stuck/double-hits
                    float overlapX = (birdSz.x/2.0f + brickSz.x/2.0f) - dx;
                    float overlapY = (birdSz.y/2.0f + brickSz.y/2.0f) - dy;
                    if (overlapX < overlapY) {
                        flyBody->position.x += (birdPos.x < brickPos.x ? -overlapX : overlapX);
                    } else {
                        flyBody->position.y += (birdPos.y < brickPos.y ? -overlapY : overlapY);
                    }
                    birdPos = flyBody->position;
                    
                    if (willDie) killBrick(br);
                }
            }
            
            // bird vs pigs
            for (Pig& pg : pigs) {
                if (pg.dead || !pg.body) continue;
                Vec2 pigPos = pg.body->position;
                Vec2 pigSz = pg.body->width;
                
                float dx = fabsf(birdPos.x - pigPos.x);
                float dy = fabsf(birdPos.y - pigPos.y);
                if (dx < (birdSz.x/2.0f + pigSz.x/2.0f) && dy < (birdSz.y/2.0f + pigSz.y/2.0f)) {
                    Vec2 vBefore = flyBody->velocity;
                    float speedBefore = vBefore.Length();
                    float dmg = (50.0f + speedBefore * 0.2f) * getDamageMult();
                    pg.hp -= dmg;
                    
                    Vec2 transfer = 0.6f * vBefore;
                    pg.body->velocity += pg.body->invMass * transfer;
                    
                    bool willDie = pg.hp <= 0.0f;
                    flyBody->velocity *= (willDie ? 0.9f : 0.5f);
                    
                    if (willDie) killPig(pg);
                }
            }
            
            // bird vs floor
            if (flyBody->position.y - birdSz.y/2.0f < floorY) {
                float impactY = fabsf(flyBody->velocity.y);
                flyBody->position.y = floorY + birdSz.y/2.0f;
                if (flyBody->velocity.y < 0.0f) flyBody->velocity.y = -flyBody->velocity.y * 0.4f;
                flyBody->velocity.x *= 0.75f;
                flyBody->angularVelocity *= 0.5f;
            }
            // bird vs walls
            if (flyBody->position.x - birdSz.x/2.0f < 18.0f) {
                flyBody->position.x = 18.0f + birdSz.x/2.0f;
                if (flyBody->velocity.x < 0.0f) flyBody->velocity.x = -flyBody->velocity.x * 0.5f;
            }
            if (flyBody->position.x + birdSz.x/2.0f > wallR) {
                flyBody->position.x = wallR - birdSz.x/2.0f;
                if (flyBody->velocity.x > 0.0f) flyBody->velocity.x = -flyBody->velocity.x * 0.5f;
            }
            
            // update bird sprite
            birdPos = flyBody->position;
            if (flyBird) {
                flyBird->setPosition(ccp(birdPos.x, birdPos.y));
                flyBird->setRotation(-flyBody->rotation * 180.0f / 3.14159f);
            }
            
            birdLifetime += dt;
            float spd = flyBody->velocity.Length();
            bool onGround = birdPos.y - birdSz.y/2.0f < floorY + 3.0f;
            if (spd < 70.0f && onGround) restTimer += dt; else restTimer = 0.0f;
            bool offScreen = birdPos.x > wallR + 60.0f || birdPos.x < -60.0f || birdPos.y < -50.0f;
            if (restTimer > 0.5f || offScreen || birdLifetime > 5.0f) {
                if (flyBird) { flyBird->removeFromParent(); flyBird = nullptr; }
                if (flyBody) {
                    removeBody(flyBody);
                    flyBody = nullptr;
                }
                flying = false;
                spawnSlingBird();
            }
        }
        
        checkWin();
    }

    void removeBody(Body* b) {
        // nuke any arbiters referencing this body or shit will crash
        for (auto it = world->arbiters.begin(); it != world->arbiters.end(); ) {
            if (it->first.body1 == b || it->first.body2 == b) {
                it = world->arbiters.erase(it);
            } else {
                ++it;
            }
        }
        world->bodies.erase(std::remove(world->bodies.begin(), world->bodies.end(), b), world->bodies.end());
        delete b;
    }

    void killBrick(Brick& br) {
        br.dead = true;
        if (br.shit) br.shit->runAction(CCSequence::create(CCFadeOut::create(0.2f), CCRemoveSelf::create(), nullptr));
        if (br.hpLbl) br.hpLbl->runAction(CCSequence::create(CCFadeOut::create(0.2f), CCRemoveSelf::create(), nullptr));
        if (br.body) {
            removeBody(br.body);
            br.body = nullptr;
        }
        score += 50;
        updateScore();
    }

    void killPig(Pig& pg) {
        pg.dead = true;
        FMODAudioEngine::sharedEngine()->playEffect("poof.mp3"_spr);
        if (pg.spr) {
            pg.spr->runAction(CCSequence::create(
                CCSpawn::create(CCFadeOut::create(0.4f), CCScaleTo::create(0.4f, 0.0f), nullptr),
                CCRemoveSelf::create(),
                nullptr
            ));
        }
        if (pg.body) {
            removeBody(pg.body);
            pg.body = nullptr;
        }
        score += 200;
        updateScore();
    }

    void updateScore() {
        if (score > g_high) {
            g_high = score;
            Mod::get()->setSavedValue<int>("high_score", g_high);
        }
        if (scoreLbl) scoreLbl->setString(fmt::format("Score: {}  Hi: {}", score, g_high).c_str());
    }

    void checkWin() {
        if (ended) return;
        bool anyAlive = false;
        for (Pig& pg : pigs) if (!pg.dead) { anyAlive = true; break; }
        // unlock next level on win
        if (!anyAlive && !ended) {
            int nextLvl = levelIdx + 2; // 1-indexed unlocks
            if (nextLvl > g_unlocked) {
                g_unlocked = nextLvl;
                Mod::get()->setSavedValue<int>("unlocked", g_unlocked);
            }
        }
        if (!anyAlive) {
            ended = true;
            score += birdsLeft * 500;
            updateScore();
            // coin
            int reward = 100 + birdsLeft * 50 + levelIdx * 25;
            g_coins += reward;
            Mod::get()->setSavedValue<int>("coins", g_coins);
            if (msgLbl) msgLbl->setString(fmt::format("VICTORY! +{} coins", reward).c_str());
            FMODAudioEngine::sharedEngine()->stopAllMusic(true);
            FMODAudioEngine::sharedEngine()->playEffect("victory.mp3"_spr);
            showReplayBtn();
            return;
        }
        if (birdsLeft <= 0 && !flying && !slingBird) {
            ended = true;
            if (msgLbl) msgLbl->setString("OUT OF BIRDS L BOZO");
            FMODAudioEngine::sharedEngine()->stopAllMusic(true);
            FMODAudioEngine::sharedEngine()->playEffect("loss.mp3"_spr);
            showReplayBtn();
        }
    }

    void showReplayBtn() {
        CCSize ws = CCDirector::sharedDirector()->getWinSize();
        CCMenu* replayMenu = CCMenu::create();
        replayMenu->setPosition(ccp(0.0f, 0.0f));
        ButtonSprite* replaySpr = ButtonSprite::create("REPLAY", 0, false, "bigFont.fnt", "GJ_button_02.png", 50.0f, 0.7f);
        CCMenuItemSpriteExtra* replayBtn = CCMenuItemSpriteExtra::create(replaySpr, this, menu_selector(AngryLayer::onReset));
        replayBtn->setPosition(ccp(ws.width / 2.0f, ws.height / 2.0f - 40.0f));
        replayMenu->addChild(replayBtn, topZ(replayMenu) + 1);
        this->addChild(replayMenu, topZ(this) + 1);
    }
};

CCLayer* makeAngryLayer(int idx) { return AngryLayer::create(idx); }

class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        g_high = Mod::get()->getSavedValue<int>("high_score", 0);
        g_unlocked = Mod::get()->getSavedValue<int>("unlocked", 1);
        if (g_unlocked < 1) g_unlocked = 1;
        g_coins = Mod::get()->getSavedValue<int>("coins", 0);
        g_dmgUp = Mod::get()->getSavedValue<int>("dmg_up", 0);
        g_birdUp = Mod::get()->getSavedValue<int>("bird_up", 0);
        CCNode* bm = this->getChildByID("bottom-menu");
        CCSprite* iconSpr = CCSprite::create("logo.png"_spr);
        if (!iconSpr) iconSpr = CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png");
        CCMenuItemSpriteExtra* b = CCMenuItemSpriteExtra::create(iconSpr, this, menu_selector(MyMenuLayer::onAngry));
        b->setID("axiom-angry-btn");
        if (bm) { bm->addChild(b); bm->updateLayout(); }
        return true;
    }

    void onAngry(CCObject*) {
        if (g_open) return;
        g_open = true;
        CCScene* sc = CCScene::create();
        sc->addChild(AngryLevelSelectLayer::create());
        CCDirector::sharedDirector()->pushScene(sc);
    }
};
