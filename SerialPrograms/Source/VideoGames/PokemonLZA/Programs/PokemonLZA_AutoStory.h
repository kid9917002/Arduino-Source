/*  Auto Story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The order of the steps is read from story_flow.json in the macro
 *  folder. Each step runs a macro, clears a story dialog, or runs a
 *  built-in action. Without a usable file the built-in flow is used.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_H
#define PokemonAutomation_PokemonLZA_AutoStory_H

#include "PokemonLZA_AutoStoryMultiLang.h"

namespace PokemonAutomation{
    class JsonObject;
namespace NintendoSwitch{
namespace PokemonLZA{


class AutoStory_Descriptor : public AutoStoryMultiLang_Descriptor{
public:
    AutoStory_Descriptor();
};


class AutoStory : public AutoStoryMultiLang{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    void restart_and_load_save(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void run_flow_step(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const JsonObject& step, size_t index, size_t count
    );
    void run_flow_action(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& action, const std::string& id, const JsonObject& step
    );
};



}
}
}
#endif
