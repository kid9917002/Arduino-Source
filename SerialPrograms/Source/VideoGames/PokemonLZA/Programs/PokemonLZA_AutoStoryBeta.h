/*  Auto Story (Beta)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Same as Auto Story, but the order of the steps is read from
 *  story_flow.json in the macro folder. Each step runs a macro, clears a
 *  story dialog, or runs a built-in action. Without a usable file the
 *  built-in flow (the same order as Auto Story) is used.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBeta_H
#define PokemonAutomation_PokemonLZA_AutoStoryBeta_H

#include "PokemonLZA_AutoStoryMultiLang.h"

namespace PokemonAutomation{
    class JsonObject;
namespace NintendoSwitch{
namespace PokemonLZA{


class AutoStoryBeta_Descriptor : public AutoStoryMultiLang_Descriptor{
public:
    AutoStoryBeta_Descriptor();
};


class AutoStoryBeta : public AutoStoryMultiLang{
public:
    using Descriptor = AutoStoryBeta_Descriptor;
    AutoStoryBeta();

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
