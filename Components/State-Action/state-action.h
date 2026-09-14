//this is the general actions with the specific actions being subclasses of the above
#include  <iostream>
#include <math.h>
#include <vector>
#include <memory>






// defentitions for action implementation
class base_action
{ public:
    virtual void act()=0;
    virtual ~base_action() = default; 
};

class complex_action{
public:
    std::unique_ptr<base_action> internal_current_action;
    void act(std::vector<base_action> included_actions,std::vector<std::string> action_order);
};
// specific action space (will be the same for both agents but will only be called)
class move:base_action{};
class grab:base_action{};
class lay:base_action{};
class shake_head:base_action{};
class idle:base_action{}; // can be used under continous invalid or unprocessibke output or state transition.
class bark:base_action{};
class get_attention:complex_action{};
class follow:complex_action{};
class get_confused:complex_action{};
class snuggle:complex_action{};
class fetch:complex_action{};
class wander:complex_action{}; 
