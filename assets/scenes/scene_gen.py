import sys
import random

def dump_entity_data(file_handle, entity_data):
    file_handle.write("{\n")
    for key, value in entity_data.items():
        file_handle.write("\"{}\" \"{}\"\n".format(key, value))
    file_handle.write("}\n")

def main():
    print("Hello World")
    print(sys.argv)

    with open(sys.argv[1], 'w') as f:
        width = int(sys.argv[2])
        # height = int(sys.argv[3])
        # depth = int(sys.argv[4])

        DIST_INCR = 500
        ANGLE_INCR = 5
        SCALE = 0.01
        """
        for i in range(width):
            for j in range(height):
                for k in range(depth):
                    s = random.random() * SCALE
                    entData = {
                        "classname": "asteroid",
                        "origin": "{} {} {}".format(DIST_INCR*i + random.random() * DIST_INCR,
                                                    DIST_INCR*j + random.random() * DIST_INCR,
                                                    DIST_INCR*k + random.random() * DIST_INCR),
                        "angles": "{} {} {}".format(random.random() * 360, random.random() * 360, random.random() * 360),
                        
                        "scale": "{} {} {}".format(s, s, s),
                        
                        "mesh" : "./resources/assets/models/asteroid_01/scene.gltf",
                        "material": "base_inst.shader",
                        "is_instanced" : "1"
                    }
                    dump_entity_data(f, entData)

        dump_entity_data(f, {
            "classname" : "camera",
            "origin" : "-3 3 -3",
            "angles" : "-60 -315 0",
            "fovy" : "30",
            "near" : "0.1",
            "far" : "10.0",
        })

        dump_entity_data(f, {
            "classname" : "camera",
            "origin" : "-10 20 -10",
            "angles" : "-60 -315 0",
            "fovy" : "30",
            "near" : "0.1",
            "far" : "100.0",
        })
        """

        aabb = {
            "min": [-30, 0, -15],
            "max": [25, 30, 5]
        }
        
        dist_x = aabb["max"][0] - aabb["min"][0]
        dist_y = aabb["max"][1] - aabb["min"][1]
        dist_z = aabb["max"][2] - aabb["min"][2]

        """
        {
        "classname" "point_light"
        "origin" "0 20 0"
        "radius" "40.0"
        "color" "1.0 1.0 1.0"
        "intensity" "0.2"
        }
        """


        # Dump model entity
        dump_entity_data(f, {
            "classname": "model",
            "origin": "0 0 0",
            "angles": "0 0 0",
            "scale": "3 3 3",
            "path": "../assets/models/sponza/glTF/Sponza.gltf"
        })

        # # Dump point light entity
        # dump_entity_data(f, {
        #     "classname": "point_light",
        #     "origin": "0 20 0",
        #     "radius": "40.0",
        #     "color": "1.0 1.0 1.0",
        #     "intensity": "0.2"
        # })

        # Dump skybox entity
        dump_entity_data(f, {
            "classname": "skybox",
            "front": "../assets/skybox/room/pz.png",
            "back": "../assets/skybox/room/nz.png",
            "top": "../assets/skybox/room/py.png",
            "bottom": "../assets/skybox/room/ny.png",
            "right": "../assets/skybox/room/px.png",
            "left": "../assets/skybox/room/nx.png"
        })

        for i in range(width):
            entData = {
                "classname": "point_light",
                "origin": "{:.2f} {:.2f} {:.2f}".format(aabb["min"][0] + random.random() * dist_x,
                                            aabb["min"][1] + random.random() * dist_y,
                                            aabb["min"][2] + random.random() * dist_z),
                "radius": "{:.2f}".format(random.random() * 5.0 + 5.0),
                "color": "{:.2f} {:.2f} {:.2f}".format(random.random(), random.random(), random.random()),
                "intensity": "1.0",                    
            }
            dump_entity_data(f, entData)




if __name__ == '__main__':
    main()