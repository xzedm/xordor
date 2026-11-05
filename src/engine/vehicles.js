// Drivable vehicles, using cannon-es's RaycastVehicle (wheels with suspension).
// The car's look comes from its shape: the part tagged "chassis" becomes the
// physics box and the parts tagged with a wheel number become the wheels.
import * as THREE from 'three';
import { CANNON } from './physics.js';

const _q = new THREE.Quaternion();
const _v = new THREE.Vector3();
const _m = new THREE.Matrix4();

export class Vehicle {
  constructor(game, rt, params) {
    this.game = game;
    this.rt = rt;
    this.params = params;
    this.steer = 0;
    this.flipTimer = 0;
    const obj = rt.obj;
    const s = obj.scale;
    const chassis = obj.children.find((c) => c.userData.part === 'chassis');
    this.wheels = obj.children.filter((c) => c.userData.wheel !== undefined).sort((a, b) => a.userData.wheel - b.userData.wheel);

    const mass = Math.max(20, rt.entity.mass > 1 ? rt.entity.mass : 150);
    this.body = new CANNON.Body({ mass, material: game.materialFor(0.05), angularDamping: 0.45, linearDamping: 0.04 });
    if (chassis) {
      chassis.geometry.computeBoundingBox();
      const bb = chassis.geometry.boundingBox;
      const half = new CANNON.Vec3(((bb.max.x - bb.min.x) / 2) * s.x, ((bb.max.y - bb.min.y) / 2) * s.y, ((bb.max.z - bb.min.z) / 2) * s.z);
      this.body.addShape(new CANNON.Box(half), new CANNON.Vec3(chassis.position.x * s.x, chassis.position.y * s.y, chassis.position.z * s.z));
    } else {
      const bb = obj.userData.localBounds;
      this.body.addShape(new CANNON.Box(new CANNON.Vec3(((bb.max.x - bb.min.x) / 2) * s.x, ((bb.max.y - bb.min.y) / 4) * s.y, ((bb.max.z - bb.min.z) / 2) * s.z)));
    }
    this.body.position.set(obj.position.x, obj.position.y, obj.position.z);
    this.body.quaternion.set(obj.quaternion.x, obj.quaternion.y, obj.quaternion.z, obj.quaternion.w);

    this.vehicle = new CANNON.RaycastVehicle({ chassisBody: this.body, indexRightAxis: 0, indexUpAxis: 1, indexForwardAxis: 2 });
    const scaleY = s.y;
    this.radius = 0.42 * Math.min(s.y, (s.x + s.z) / 2);
    const rest = 0.3 * scaleY;
    for (const w of this.wheels) {
      this.vehicle.addWheel({
        radius: this.radius,
        directionLocal: new CANNON.Vec3(0, -1, 0),
        axleLocal: new CANNON.Vec3(-1, 0, 0),
        suspensionStiffness: 40,
        suspensionRestLength: rest,
        frictionSlip: params.grip,
        dampingRelaxation: 2.4,
        dampingCompression: 4.5,
        maxSuspensionForce: 1e6,
        rollInfluence: 0.02,
        maxSuspensionTravel: 0.35 * scaleY,
        customSlidingRotationalSpeed: -30,
        useCustomSlidingRotationalSpeed: true,
        chassisConnectionPointLocal: new CANNON.Vec3(w.position.x * s.x, w.position.y * s.y + rest, w.position.z * s.z),
      });
    }
    if (!this.wheels.length) {
      // A shape without wheels still drives, with four hidden wheels at its corners.
      const bb = obj.userData.localBounds;
      for (const [x, z] of [[-1, 1], [1, 1], [-1, -1], [1, -1]]) {
        this.vehicle.addWheel({
          radius: this.radius, directionLocal: new CANNON.Vec3(0, -1, 0), axleLocal: new CANNON.Vec3(-1, 0, 0), suspensionStiffness: 40,
          suspensionRestLength: rest, frictionSlip: params.grip, dampingRelaxation: 2.4, dampingCompression: 4.5, maxSuspensionForce: 1e6,
          rollInfluence: 0.02, maxSuspensionTravel: 0.35, customSlidingRotationalSpeed: -30, useCustomSlidingRotationalSpeed: true,
          chassisConnectionPointLocal: new CANNON.Vec3(x * bb.max.x * s.x * 0.8, bb.min.y * s.y + rest + this.radius, z * bb.max.z * s.z * 0.8),
        });
      }
    }
    this.vehicle.addToWorld(game.world);
    this.park();
  }

  get position() {
    return this.body.position;
  }

  /** Speed in meters per second. */
  get speed() {
    return this.body.velocity.length();
  }

  /** Signed speed along the car's nose. */
  get forwardSpeed() {
    const f = this.body.quaternion.vmult(new CANNON.Vec3(0, 0, 1));
    return f.dot(this.body.velocity);
  }

  get heading() {
    const f = this.body.quaternion.vmult(new CANNON.Vec3(0, 0, 1));
    return Math.atan2(f.x, f.z);
  }

  park() {
    const n = this.vehicle.wheelInfos.length;
    for (let i = 0; i < n; i++) {
      this.vehicle.applyEngineForce(0, i);
      this.vehicle.setBrake(3, i);
    }
  }

  /** Throttle -1..1, steer -1..1 (right is positive), brake true/false. */
  drive(h, throttle, steerInput, brake) {
    const v = this.vehicle;
    const n = v.wheelInfos.length;
    const mass = this.body.mass;
    const top = this.params.speed;
    const fwd = this.forwardSpeed;
    // Forces are per wheel, in newtons: about 12 m/s² of push and 15 m/s² of braking.
    const maxForce = mass * 6;
    let engine = throttle * maxForce;
    let brakeForce = brake ? mass * 3.8 : 0;
    // Pressing back while still rolling forward brakes first, then reverses.
    if (throttle < 0 && fwd > 1.5) { engine = 0; brakeForce = mass * 3; }
    if (throttle > 0 && fwd < -1.5) { engine = 0; brakeForce = mass * 3; }
    if (throttle < 0) engine *= 0.5;
    if ((throttle > 0 && fwd > top) || (throttle < 0 && fwd < -top * 0.4)) engine = 0;
    if (!throttle && !brake) brakeForce = mass * 0.12;
    // Less steering at high speed keeps the car controllable.
    const maxSteer = 0.55 / (1 + Math.abs(fwd) / 25);
    this.steer += (-steerInput * maxSteer - this.steer) * Math.min(1, h * 9);
    for (let i = 0; i < n; i++) {
      const front = i < 2;
      v.applyEngineForce(front ? 0 : -engine, i);
      v.setSteeringValue(front ? this.steer : 0, i);
      v.setBrake(brakeForce, i);
    }
  }

  /** Turns the car back on its wheels when it has been upside down for a while. */
  checkFlipped(h) {
    const up = this.body.quaternion.vmult(new CANNON.Vec3(0, 1, 0));
    if (up.y < 0.25 && this.speed < 3) this.flipTimer += h;
    else this.flipTimer = 0;
    if (this.flipTimer > 1.5) {
      this.flipTimer = 0;
      this.reset(new THREE.Vector3(this.body.position.x, this.body.position.y + 1.5, this.body.position.z), this.heading);
    }
  }

  reset(position, yaw = 0) {
    this.body.position.set(position.x, position.y, position.z);
    this.body.velocity.set(0, 0, 0);
    this.body.angularVelocity.set(0, 0, 0);
    _q.setFromAxisAngle(new THREE.Vector3(0, 1, 0), yaw);
    this.body.quaternion.set(_q.x, _q.y, _q.z, _q.w);
    this.steer = 0;
  }

  /** Copies the physics back to the 3D model, wheels included. */
  sync() {
    const obj = this.rt.obj;
    const b = this.body;
    obj.position.set(b.position.x, b.position.y, b.position.z);
    obj.quaternion.set(b.quaternion.x, b.quaternion.y, b.quaternion.z, b.quaternion.w);
    obj.updateMatrixWorld();
    _m.copy(obj.matrixWorld).invert();
    const objQ = obj.quaternion.clone().invert();
    for (let i = 0; i < this.wheels.length; i++) {
      this.vehicle.updateWheelTransform(i);
      const t = this.vehicle.wheelInfos[i].worldTransform;
      const w = this.wheels[i];
      w.position.copy(_v.set(t.position.x, t.position.y, t.position.z).applyMatrix4(_m));
      w.quaternion.copy(objQ).multiply(_q.set(t.quaternion.x, t.quaternion.y, t.quaternion.z, t.quaternion.w));
    }
  }

  dispose() {
    this.vehicle.removeFromWorld(this.game.world);
  }
}
