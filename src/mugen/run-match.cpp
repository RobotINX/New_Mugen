#include <string>
#include <ostream>

#include "game.h"
#include "util/graphics/bitmap.h"
#include "stage.h"
#include "factory/font_render.h"
#include "util/console.h"
#include "options.h"
#include "behavior.h"
#include "exception.h"
#include "util/message-queue.h"
#include "util/init.h"
#include "config.h"
#include "util/sound/music.h"
#include "util/exceptions/shutdown_exception.h"
#include "util/system.h"
#include "character.h"

using std::string;
using std::ostringstream;

namespace PaintownUtil = ::Util;

namespace Mugen{

static const int DEFAULT_WIDTH = 320;
static const int DEFAULT_HEIGHT = 240;

enum MugenInput{
    SlowDown,
    SpeedUp,
    Pause,
    ForwardFrame,
    NormalSpeed,
    ToggleDebug,
    QuitGame,
    SetHealth,
    ShowFps,
    ToggleConsole,
};

class EscapeMenu{
public:
    class ResumeException : public std::exception{
    public:
        ResumeException(){
        }
        ~ResumeException() throw(){
        }
    };
    EscapeMenu(RunMatchOptions & options):
    enabled(false),
    options(options){
        class Resume : public BaseMenuItem {
        public:
            
            Resume(){
                optionName = "Resume";
                currentValue = "(Esc)";
            }

            bool next(){
                return false;
            }

            bool previous(){
                return false;
            }
            
            bool isRunnable() const{
                return true;
            }

            void run(){
                throw ResumeException();
            }
        };
        class Exit : public BaseMenuItem {
        public:
            Exit(OptionMenu * menu):
            menu(menu){
                optionName = "Exit Match";
                currentValue = "(Enter)";
            }

            bool next(){
                return false;
            }

            bool previous(){
                return false;
            }
            
            bool isRunnable() const{
                return true;
            }

            void run(){
                if (menu->confirmDialog("Are you sure you want to exit?", false, Graphics::makeColor(0,0,0), 128, true)){
                    throw QuitGameException();
                }
            }
            
            OptionMenu * menu;
        };
        std::vector<PaintownUtil::ReferenceCount<Gui::ScrollItem> > list;
    
        // Add empty list for now
        menu = PaintownUtil::ReferenceCount<OptionMenu>(new OptionMenu(list));
        
        // Create list
        list.push_back(PaintownUtil::ReferenceCount<Gui::ScrollItem>(new Resume()));
        if (options.getPlayer1Behavior() != NULL){
            list.push_back(OptionMenu::getPlayerKeys(0, "Player 1", menu));
        }
        if (options.getPlayer2Behavior() != NULL){
            list.push_back(OptionMenu::getPlayerKeys(1, "Player 2", menu));
        }

        /* There is a circular reference here if we use just `menu'.
         *   menu -> options -> scroll item -> Exit -> menu
         * So to break the circularity we use a raw pointer to the original menu.
         */
        list.push_back(PaintownUtil::ReferenceCount<Gui::ScrollItem>(new Exit(menu.raw())));
        
        // Now update it
        menu->updateList(list);
        
        menu->setName("PAUSED");
        menu->setRenderBackground(false);
        menu->setClearAlpha(128);
    }

    ~EscapeMenu(){
    }

    void act(){
        menu->act();
    }
    
    void draw(const Graphics::Bitmap & work){
        if (enabled){
            menu->draw(work);
        }
    }
    
    void drawFirst(const Graphics::Bitmap & work){
        PaintownUtil::ReferenceCount<Graphics::Bitmap> copy = PaintownUtil::ReferenceCount<Graphics::Bitmap>(new Graphics::Bitmap(work, true));
        menu->updateScreenCapture(copy);
    }
    
    void toggle(){
        if (!options.isDemoMode()){
            enabled = !enabled;
            if (!enabled){
                menu->reset();
            }
        }
    }
    
    bool isActive() const {
        return enabled;
    }
    
    void up(){
        if (enabled){
            menu->up();
        }
    }
    void down(){
        if (enabled){
            menu->down();
        }
    }
    void left(){
        if (enabled){
            menu->left();
        }
    }
    void right(){
        if (enabled){
            menu->right();
        }
    }
    void enter(){
        if (enabled){
            try {
                menu->enter();
            } catch (const OptionMenu::KeysChangedException & ex){
                switch (ex.getType()){
                    case Player1:
                        if (options.getPlayer1Behavior() != NULL){
                            options.getPlayer1Behavior()->updateKeys(Mugen::getPlayer1Keys(), Mugen::getPlayer1InputLeft());
                        }
                        break;
                    case Player2:
                        if (options.getPlayer2Behavior() != NULL){
                            options.getPlayer2Behavior()->updateKeys(Mugen::getPlayer2Keys(), Mugen::getPlayer2InputLeft());
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
protected:
    bool enabled;
    PaintownUtil::ReferenceCount<OptionMenu> menu;
    RunMatchOptions & options;
};

static void runEscape(EscapeMenu & menu){
    class Logic : public PaintownUtil::Logic{
    public:
        Logic(EscapeMenu & menu):
        isDone(false),
        menu(menu),
        player1Input(Mugen::getPlayer1Keys(20)),
        player2Input(Mugen::getPlayer2Keys(20)){
        }
        ~Logic(){
        }
        
        bool isDone;
        EscapeMenu & menu;
        InputMap<Keys> player1Input;
        InputMap<Keys> player2Input;
    
        double ticks(double system){
            return Mugen::Util::gameTicks(system);
        }

        void run(){
            InputSource input1;
            InputSource input2;
            std::vector<InputMap<Mugen::Keys>::InputEvent> out1 = InputManager::getEvents(player1Input, input1);
            std::vector<InputMap<Mugen::Keys>::InputEvent> out2 = InputManager::getEvents(player2Input, input2);
            out1.insert(out1.end(), out2.begin(), out2.end());
            for (std::vector<InputMap<Mugen::Keys>::InputEvent>::iterator it = out1.begin(); it != out1.end(); it++){
                const InputMap<Mugen::Keys>::InputEvent & event = *it;
                if (!event.enabled){
                    continue;
                }

                if (event[Esc]){
                    isDone = true;
                }

                if (event[Up]){
                    menu.up();
                }
                if (event[Down]){
                    menu.down();
                }
                if (event[Left]){
                    menu.left();
                }
                if (event[Right]){
                    menu.right();
                }
                if (event[Start]){
                    try {
                        menu.enter();
                    } catch (const EscapeMenu::ResumeException & ex){
                        isDone = true;
                    }
                }
            }
            
            // Act out
            menu.act();
        }

        bool done(){
            return isDone;
        }
    };
    
    class Draw : public PaintownUtil::Draw {
    public:
        Draw(EscapeMenu & menu):
        menu(menu){
        }

        EscapeMenu & menu;
        
        void drawFirst(const Graphics::Bitmap & screen){
            menu.drawFirst(screen);
        }
        
        void draw(const Graphics::Bitmap & screen){
            menu.draw(screen);
            screen.BlitToScreen();
        }
    };
    
    menu.toggle();
    Logic logic(menu);
    Draw draw(menu);
    PaintownUtil::standardLoop(logic, draw);
    menu.toggle();
}
class LogicDraw: public PaintownUtil::Logic, public PaintownUtil::Draw {
    public:
        LogicDraw(Mugen::Stage * stage, bool & show_fps, Console::Console & console, RunMatchOptions & options):
        endMatch(false),
        gameSpeed(Data::getInstance().getGameSpeed()),
        stage(stage),
        show_fps(show_fps),
        console(console),
        gameTicks(0),
        options(options),
        show(true),
        showGameSpeed(0),
        escapeMenu(options){
            gameInput.set(Keyboard::Key_F1, SlowDown);
            gameInput.set(Keyboard::Key_F2, SpeedUp);
            gameInput.set(Keyboard::Key_F3, NormalSpeed);
            gameInput.set(Keyboard::Key_F4, ToggleDebug);
            gameInput.set(Keyboard::Key_F6, Pause);
            gameInput.set(Keyboard::Key_F7, ForwardFrame);
            gameInput.set(Keyboard::Key_ESC, QuitGame);
            gameInput.set(Joystick::Quit, QuitGame);
            gameInput.set(Keyboard::Key_F5, SetHealth);
            gameInput.set(Keyboard::Key_F9, ShowFps);
            gameInput.set(Keyboard::Key_TILDE, ToggleConsole);

            Global::registerInfo(&messages);
        }

        virtual ~LogicDraw(){
            Global::unregisterInfo(&messages);
        }

        InputMap<MugenInput> gameInput;
        bool endMatch;
        double gameSpeed;
        Mugen::Stage * stage;
        bool & show_fps;
        Console::Console & console;
        /* global info messages will appear in the console */
        MessageQueue messages;

        double gameTicks;
        RunMatchOptions & options;
        bool show;
        int showGameSpeed;
        
        EscapeMenu escapeMenu;

        void doInput(){
            class Handler: public InputHandler<MugenInput> {
                public:
                    Handler(LogicDraw & logic):
                        logic(logic){
                        }

                    LogicDraw & logic;

                    void release(const MugenInput & out, Keyboard::unicode_t code){
                    }

                    void press(const MugenInput & out, Keyboard::unicode_t code){
                        const int DISPLAY_GAME_SPEED_TIME = 100;
                        switch (out){
                            case SlowDown: {
                                logic.showGameSpeed = DISPLAY_GAME_SPEED_TIME;
                                logic.gameSpeed -= 0.1;
                                if (logic.gameSpeed < 0.1){
                                    logic.gameSpeed = 0.1;
                                }
                                break;
                            }
                            case SpeedUp: {
                                logic.showGameSpeed = DISPLAY_GAME_SPEED_TIME;
                                logic.gameSpeed += 0.1;
                                break;
                            }
                            case ForwardFrame: {
                                logic.showGameSpeed = DISPLAY_GAME_SPEED_TIME;
                                logic.gameSpeed = 0;
                                logic.gameTicks = 1;
                                break;
                            }
                            case Pause: {
                                logic.showGameSpeed = DISPLAY_GAME_SPEED_TIME;
                                logic.gameSpeed = 0;
                                break;
                            }
                            case NormalSpeed: {
                                logic.showGameSpeed = DISPLAY_GAME_SPEED_TIME;
                                logic.gameSpeed = 1;
                                break;
                            }
                            case ToggleDebug: {
                                logic.stage->toggleDebug(0);
                                break;
                            }
                            case QuitGame: {
                                if (!logic.options.isDemoMode()){
                                    runEscape(logic.escapeMenu);
                                }
                                break;
                            }
                            case SetHealth: {
                                logic.stage->setPlayerHealth(1);
                                break;
                            }
                            case ShowFps: {
                                logic.show_fps = ! logic.show_fps;
                                break;
                            }
                            case ToggleConsole: {
                                logic.console.toggle();
                                break;
                            }
                        }
                    }
            };

            Handler handler(*this);
            InputManager::handleEvents(gameInput, InputSource(), handler);
        }

        virtual void run(){
            gameTicks += gameSpeed;
            // Do stage logic catch match exception to handle the next match
            while (gameTicks > 0){
                gameTicks -= 1;
                stage->logic();
            }
            while (messages.hasAny()){
                console.addLine(messages.get());
            }
            console.act();
            endMatch = stage->isMatchOver();

            if (showGameSpeed > 0){
                showGameSpeed -= 1;
            }

            if (console.isActive()){
                try{
                    console.doInput();
                } catch (Exception::Return & r){
                    throw QuitGameException();
                }
            } else {
                doInput();
            }

            options.act();
        }

        virtual bool done(){
            return endMatch;
        }

        virtual double ticks(double system){
            return Mugen::Util::gameTicks(system);
        }

        virtual void draw(const Graphics::Bitmap & screen){
            if (show_fps){
                if (Global::second_counter % 2 == 0){
                    if (show){
                        Global::debug(0) << "FPS: " << getFps() << std::endl;
                        show = false;
                    }
                } else {
                    show = true;
                }
            }

            if (stage->isZoomed()){
                Graphics::Bitmap work(DEFAULT_WIDTH, DEFAULT_HEIGHT);
                stage->render(&work);
                // Global::debug(0) << "X1 " << stage->zoomX1() << " Y1 " << stage->zoomY1() << " X2 " << stage->zoomX2() << " Y2 " << stage->zoomY2() << std::endl;
                work.Stretch(screen, stage->zoomX1(), stage->zoomY1(), stage->zoomX2() - stage->zoomX1(), stage->zoomY2() - stage->zoomY1(), 0, 0, screen.getWidth(), screen.getHeight());
            } else {
                Graphics::StretchedBitmap work(DEFAULT_WIDTH, DEFAULT_HEIGHT, screen, Graphics::qualityFilterName(::Configuration::getQualityFilter()));
                work.start();
                stage->render(&work);
                options.draw(work);
                work.finish();
            }

            FontRender * render = FontRender::getInstance();
            render->render(&screen);
            console.draw(screen);
            if (showGameSpeed > 0){
                const ::Font & font = ::Font::getDefaultFont(15, 15);
                font.printf(1, screen.getHeight() - font.getHeight() - 5, Graphics::makeColor(255, 255, 255), screen, "Game speed %f", 0, gameSpeed);
            }
            screen.BlitToScreen();
        }
};

void Game::runMatch(Mugen::Stage * stage, const std::string & musicOverride, RunMatchOptions options){
    //Music::changeSong();
    // *NOTE according to bgs.txt they belong in sound directory
    Filesystem::AbsolutePath file;
    try{
        if (musicOverride.empty()){
            Music::loadSong(Storage::instance().find(Filesystem::RelativePath(Mugen::Data::getInstance().getDirectory().path() + "/sound/" + stage->getMusic())).path());
        } else {
            Music::loadSong(Storage::instance().find(Filesystem::RelativePath(Mugen::Data::getInstance().getDirectory().path() + "/sound/" + musicOverride)).path());
        }
        Music::pause();
        Music::play();
    } catch (const Filesystem::NotFound & fail){
        Global::debug(0) << "Could not load music because " << fail.getTrace() << std::endl;
        Music::changeSong();
    }
    /* Ignore volume for now */
    //Music::setVolume(stage->getMusicVolume());
    /*
    Music::pause();
    Music::play();
    */

    Console::Console console(150);
    {
        class CommandQuit: public Console::Command {
        public:
            CommandQuit(){
            }

            string act(const string & line){
                throw ShutdownException();
            }

            string getDescription() const {
                return "quit - quit the game entirely";
            }
        };

        class CommandMemory: public Console::Command {
        public:
            CommandMemory(){
            }

            string getDescription() const {
                return "memory - current memory usage";
            }

            string act(const string & line){
                ostringstream out;
                out << "Memory usage: " << PaintownUtil::niceSize(System::memoryUsage()) << "\n";
                return out.str();
            }
        };

        class CommandHelp: public Console::Command {
        public:
            CommandHelp(const Console::Console & console):
            console(console){
            }

            const Console::Console & console;

            string getDescription() const {
                return "help - Show this help";
            }

            string act(const string & line){
                ostringstream out;
                const std::vector<PaintownUtil::ReferenceCount<Console::Command> > commands = console.getCommands();
                for (std::vector<PaintownUtil::ReferenceCount<Console::Command> >::const_iterator it = commands.begin(); it != commands.end(); it++){
                    PaintownUtil::ReferenceCount<Console::Command> command = *it;
                    out << command->getDescription() << "\n";
                }
                return out.str();
            }
        };

        class CommandDebug: public Console::Command {
        public:
            CommandDebug(Mugen::Stage * stage):
            stage(stage){
            }

            Mugen::Stage * stage;

            string getDescription() const {
                return "debug [#] - Enable/disable debug for player # starting from 0";
            }

            string act(const string & line){
                std::istringstream data(line);
                int number = -1;
                string ignore;
                data >> ignore >> number;
                stage->toggleDebug(number);
                std::ostringstream out;
                out << "Enabled debug for " << number;
                return out.str();
            }
        };

        class CommandChangeState: public Console::Command {
        public:
            CommandChangeState(Mugen::Stage * stage):
            stage(stage){
            }

            Mugen::Stage * stage;

            string getDescription() const {
                return "change-state [player #] [state #] - Change player to a state";
            }

            string act(const string & line){
                std::istringstream input(line);
                string command;
                int character = 0;
                int state = 0;
                input >> command >> character >> state;
                std::vector<Character*> players = stage->getPlayers();
                int count = 0;
                if (character < players.size()){
                    Character * player = players[character];
                    player->changeState(*stage, state);
                    std::ostringstream out;
                    out << "Changed state for " << player->getDisplayName() << " to " << state;
                    return out.str();
                }
                std::ostringstream out;
                out << "No such player " << character;
                return out.str();
            }
        };

        class CommandKill: public Console::Command {
        public:
            CommandKill(Mugen::Stage * stage):
            stage(stage){
            }

            Mugen::Stage * stage;

            string getDescription() const {
                return "kill # - Set a players health to 0, killing him"; 
            }
            
            string act(const string & line){
                std::istringstream input(line);
                string command;
                int character = 0;
                input >> command >> character;
                std::vector<Character*> players = stage->getPlayers();
                int count = 0;
                if (character < players.size()){
                    Character * player = players[character];
                    player->setHealth(0);
                    std::ostringstream out;
                    out << "Killed player " << character;
                    return out.str();
                }
                std::ostringstream out;
                out << "No such player " << character;
                return out.str();
            }

        };

        class CommandRecord: public Console::Command {
        public:
            CommandRecord(Mugen::Stage * stage):
            stage(stage){
            }

            Mugen::Stage * stage;

            string getDescription() const {
                return "record - Record moves to a file"; 
            }

            string act(const string & line){
                std::vector<Character*> players = stage->getPlayers();
                int count = 0;
                for (std::vector<Character*>::iterator it = players.begin(); it != players.end(); it++){
                    count += 1;
                    Character * player = *it;
                    player->startRecording(count);
                }
                return "Recording";
            }
        };

        console.addCommand("quit", PaintownUtil::ReferenceCount<Console::Command>(new CommandQuit()));
        console.addAlias("exit", "quit");
        console.addCommand("help", PaintownUtil::ReferenceCount<Console::Command>(new CommandHelp(console)));
        console.addCommand("memory", PaintownUtil::ReferenceCount<Console::Command>(new CommandMemory()));
        console.addCommand("kill", PaintownUtil::ReferenceCount<Console::Command>(new CommandKill(stage)));
        console.addCommand("record", PaintownUtil::ReferenceCount<Console::Command>(new CommandRecord(stage)));
        console.addCommand("debug", PaintownUtil::ReferenceCount<Console::Command>(new CommandDebug(stage)));
        console.addCommand("change-state", PaintownUtil::ReferenceCount<Console::Command>(new CommandChangeState(stage)));
    }

    bool show_fps = false;

    LogicDraw all(stage, show_fps, console, options);

    PaintownUtil::standardLoop(all, all);
}

}