#include <vk_types.h>
#include <SDL2/SDL_events.h>

class Camera {
public:
  glm::vec3 velocity;
  glm::vec3 position;
  //vert rotation
  float pitch {0.f};
  //hori rotation
  float yaw {0.f};

  glm::mat4 getViewMatrix();
  glm::mat4 getRotationMatrix();

  void processSDLEvent(SDL_Event& e);

  void update();
};
