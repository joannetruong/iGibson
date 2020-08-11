#!/usr/bin/env python

from gibson2.core.simulator import Simulator
from gibson2.core.physics.scene import BuildingScene, StadiumScene, iGSDFScene
from gibson2.core.physics.robot_locomotors import Turtlebot, Husky, Ant, Humanoid, JR2, JR2_Kinova
import yaml
from gibson2.utils.utils import parse_config
import os
import gibson2

from gibson2.utils.assets_utils import download_assets, download_demo_data

config = parse_config(os.path.join(gibson2.root_path, '../test/test.yaml'))

def test_import_igsdf():
    
    scene = iGSDFScene('Beechwood_0')
    s = Simulator(mode='iggui', image_width=640,
                 image_height=480,)
    s.import_scene(scene)
    print("good2")

    turtlebot1 = Turtlebot(config)
    s.import_robot(turtlebot1)
    turtlebot1.set_position([0.5, 0, 3.5])
    for i in range(150000000):
        s.step()

    s.disconnect()
    print("end")

def main():
    test_import_igsdf()

if __name__ == "__main__":
    main()