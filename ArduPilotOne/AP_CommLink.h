/*
 * AP_CommLink.h
 * Copyright (C) James Goppert 2010 <james.goppert@gmail.com>
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AP_CommLink_H
#define AP_CommLink_H

#include <AP_Common.h>
#include "ArduPilotOne.h"

namespace apo
{

// forward declarations
class ArduPilotOne;

/// CommLink class
class AP_CommLink
{
public:
    enum {
        MSG_HEARTBEAT,
        MSG_ATTITUDE,
        MSG_LOCATION,
        MSG_SERVO_OUT,
    };

    enum {
        SEVERITY_LOW,
        SEVERITY_MED,
        SEVERITY_HIGH
    };

    AP_CommLink(BetterStream * link) : _link(link) {
    }
    virtual void send() = 0;
    virtual void receive() = 0;
    virtual void sendMessage(uint8_t id, uint32_t param=0) = 0;
    virtual void sendText(uint8_t severity, const char *str) = 0;
    virtual void sendText(uint8_t severity, const prog_char_t *str) = 0;
    virtual void acknowledge(uint8_t id, uint8_t sum1, uint8_t sum2) = 0;
    virtual void sendParameters() = 0;
    virtual void requestCmds() = 0;

protected:
    BetterStream * _link;
};

class MavlinkComm : public AP_CommLink
{
public:
    MavlinkComm(BetterStream * link, ArduPilotOne * apo) :
        AP_CommLink(link),
        _apo(apo),

        // options
        _useRelativeAlt(true),

        // commands
        _sendingCmds(false),
        _receivingCmds(false),
        _cmdTimeLastSent(millis()),
        _cmdCount(0),
        _cmdDestSysId(0),
        _cmdDestCompId(0),
        _cmdRequestIndex(0),
        _cmdMax(200),

        // parameters
        _parameterCount(0),
        _queuedParameter(NULL),
        _queuedParameterIndex(0) {
        switch(_nChannels) {
        case 0:
            mavlink_comm_0_port = link;
            _channel = MAVLINK_COMM_0;
            _nChannels++;
            break;
        case 1:
            mavlink_comm_1_port = link;
            _channel = MAVLINK_COMM_1;
            _nChannels++;
            break;
        default:
            // signal that number of channels exceeded
            _channel = MAVLINK_COMM_NB_HIGH;
            break;
        }
    }

    virtual void send() {
        // if number of channels exceeded return
        if (_channel == MAVLINK_COMM_NB_HIGH) return;
    }

    void sendMessage(uint8_t id, uint32_t param = 0) {
        // if number of channels exceeded return
        if (_channel == MAVLINK_COMM_NB_HIGH) return;


        uint64_t timeStamp = micros();

        switch(id) {

        case MSG_HEARTBEAT: {
            mavlink_msg_heartbeat_send(
                _channel,
                mavlink_system.type,
                MAV_AUTOPILOT_ARDUPILOTMEGA);
            break;
        }

        case MSG_ATTITUDE: {
            mavlink_msg_attitude_send(
                _channel,
                timeStamp,
                _apo->navigator()->roll,
                _apo->navigator()->pitch,
                _apo->navigator()->yaw,
                _apo->navigator()->rollRate,
                _apo->navigator()->pitchRate,
                _apo->navigator()->yawRate);
            break;
        }

        case MSG_LOCATION: {
            mavlink_msg_global_position_int_send(
                _channel,
                _apo->navigator()->latInt,
                _apo->navigator()->lonInt,
                _apo->navigator()->altInt * 10,
                _apo->navigator()->vN,
                _apo->navigator()->vE,
                _apo->navigator()->vD);
            break;
        }

        case MSG_SERVO_OUT: {
            int16_t ch[8];
            for (int i=0; i<8; i++) ch[i] = 0;
            for (int i=0; i<8 && i<_apo->rc().getSize(); i++) ch[i] = 10000*_apo->rc()[i]->getNormalized();
            //_apo->getDebug().printf_P(PSTR("SERVO_OUT: ch 1: %d ch2 : %d"),ch[0],ch[1]);
            mavlink_msg_rc_channels_scaled_send(_channel,ch[0],ch[1],ch[2],ch[3],ch[4],ch[5],ch[6],ch[7],255);
            break;
        }


        }

    }

    virtual void receive() {
        // if number of channels exceeded return
        if (_channel == MAVLINK_COMM_NB_HIGH) return;

        // receive new packets
        mavlink_message_t msg;
        mavlink_status_t status;

        // process received bytes
        while(comm_get_available(_channel)) {
            _apo->getDebug().printf("receiving data");
            uint8_t c = comm_receive_ch(_channel);

            // Try to get a new message
            if(mavlink_parse_char(_channel, c, &msg, &status)) _handleMessage(&msg);
        }

        // Update packet drops counter
        _packetDrops += status.packet_rx_drop_count;
    }

    void sendText(uint8_t severity, const char *str) {
        mavlink_msg_statustext_send(_channel,severity,(const int8_t*) str);
    }

    void sendText(uint8_t severity, const prog_char_t *str) {
        mavlink_statustext_t m;
        uint8_t i;
        for (i=0; i<sizeof(m.text); i++) {
            m.text[i] = pgm_read_byte((const prog_char *)(str++));
        }
        if (i < sizeof(m.text)) m.text[i] = 0;
        sendText(severity, (const char *)m.text);
    }

    void acknowledge(uint8_t id, uint8_t sum1, uint8_t sum2) {
    }

    /**
     * sends parameters one at a time
     */
    void sendParameters() {
        // Check to see if we are sending parameters
        while (NULL != _queuedParameter) {
            AP_Var      *vp;
            float       value;

            // copy the current parameter and prepare to move to the next
            vp = _queuedParameter;
            _queuedParameter = _queuedParameter->next();

            // if the parameter can be cast to float, report it here and break out of the loop
            value = vp->cast_to_float();
            if (!isnan(value)) {

                char paramName[_paramNameLengthMax];
                vp->copy_name(paramName, sizeof(paramName));

                mavlink_msg_param_value_send(
                    _channel,
                    (int8_t*)paramName,
                    value,
                    _countParameters(),
                    _queuedParameterIndex);

                _queuedParameterIndex++;
                break;
            }
        }

    }

    /**
     * request commands one at a time
     */
    void requestCmds() {
        // request cmds one by one
        if (_receivingCmds && _cmdRequestIndex <= _cmdCount) {
            mavlink_msg_waypoint_request_send(
                _channel,
                _cmdDestSysId,
                _cmdDestCompId,
                _cmdRequestIndex);
        }
    }

private:

    // constants
    static float _radiusEarth;

    // options
    bool _useRelativeAlt;

    // commands
    bool _sendingCmds;
    bool _receivingCmds;
    uint16_t _cmdTimeLastSent;
    uint16_t _cmdTimeLastReceived;
    AP_Uint16 _cmdCount;
    uint16_t _cmdDestSysId;
    uint16_t _cmdDestCompId;
    uint16_t _cmdRequestIndex;
    AP_Uint16 _cmdIndex;
    Vector<mavlink_command_t *> _cmdList;

    // parameters
    static uint8_t _paramNameLengthMax;
    uint16_t _parameterCount;
    AP_Var * _queuedParameter;
    uint16_t _queuedParameterIndex;
    uint16_t _cmdMax;

    // links
    ArduPilotOne * _apo;

    // channel
    Location _home;  // TODO: move this to guidance
    mavlink_channel_t _channel;
    uint16_t _packetDrops;
    static uint8_t _nChannels;

    void _handleMessage(mavlink_message_t * msg) {
        // TODO: move these to guidance
        struct     Location tell_command;                // command for telemetry

        switch(msg->msgid) {
            _apo->getDebug().printf_P(PSTR("message received: %d"),msg->msgid);

        case MAVLINK_MSG_ID_GPS_RAW: {
            // decode
            mavlink_gps_raw_t packet;
            mavlink_msg_gps_raw_decode(msg, &packet);

            _apo->navigator()->latInt = packet.lat;
            _apo->navigator()->lonInt = packet.lon;
            _apo->navigator()->altInt = packet.alt;
            _apo->navigator()->yaw = packet.hdg;
            _apo->navigator()->groundSpeed = packet.v;
            _apo->navigator()->airSpeed = packet.v;
            break;
        }

        case MAVLINK_MSG_ID_ATTITUDE: {
            // decode
            mavlink_attitude_t packet;
            mavlink_msg_attitude_decode(msg, &packet);

            // set dcm hil sensor
            _apo->navigator()->roll = packet.roll;
            _apo->navigator()->pitch = packet.pitch;
            _apo->navigator()->yaw = packet.yaw;
            _apo->navigator()->rollRate = packet.rollspeed;
            _apo->navigator()->pitchRate = packet.pitchspeed;
            _apo->navigator()->yawRate = packet.yawspeed;
            break;
        }

        case MAVLINK_MSG_ID_ACTION: {
            // decode
            mavlink_action_t packet;
            mavlink_msg_action_decode(msg, &packet);
            if (_checkTarget(packet.target,packet.target_component)) break;

            // do action
            sendText(SEVERITY_LOW,PSTR("action received"));
            switch(packet.action) {

            case MAV_ACTION_LAUNCH:
                //set_mode(TAKEOFF);
                break;

            case MAV_ACTION_RETURN:
                //set_mode(RTL);
                break;

            case MAV_ACTION_EMCY_LAND:
                //set_mode(LAND);
                break;

            case MAV_ACTION_HALT:
                //do_loiter_at_location();
                break;

                /* No mappable implementation in APM 2.0
                case MAV_ACTION_MOTORS_START:
                case MAV_ACTION_CONFIRM_KILL:
                case MAV_ACTION_EMCY_KILL:
                case MAV_ACTION_MOTORS_STOP:
                case MAV_ACTION_SHUTDOWN:
                    break;
                */

            case MAV_ACTION_CONTINUE:
                //process_next_command();
                break;

            case MAV_ACTION_SET_MANUAL:
                //set_mode(MANUAL);
                break;

            case MAV_ACTION_SET_AUTO:
                //set_mode(AUTO);
                break;

            case MAV_ACTION_STORAGE_READ:
                AP_Var::load_all();
                break;

            case MAV_ACTION_STORAGE_WRITE:
                AP_Var::save_all();
                break;

            case MAV_ACTION_CALIBRATE_RC:
                break;
                //trim_radio();
                break;

            case MAV_ACTION_CALIBRATE_GYRO:
            case MAV_ACTION_CALIBRATE_MAG:
            case MAV_ACTION_CALIBRATE_ACC:
            case MAV_ACTION_CALIBRATE_PRESSURE:
            case MAV_ACTION_REBOOT:  // this is a rough interpretation
                //startup_IMU_ground();
                break;

                /*    For future implemtation
                case MAV_ACTION_REC_START: break;
                case MAV_ACTION_REC_PAUSE: break;
                case MAV_ACTION_REC_STOP: break;
                */

                /* Takeoff is not an implemented flight mode in APM 2.0
                case MAV_ACTION_TAKEOFF:
                    set_mode(TAKEOFF);
                    break;
                */

            case MAV_ACTION_NAVIGATE:
                //set_mode(AUTO);
                break;

                /* Land is not an implemented flight mode in APM 2.0
                case MAV_ACTION_LAND:
                    set_mode(LAND);
                    break;
                */

            case MAV_ACTION_LOITER:
                //set_mode(LOITER);
                break;

            default:
                break;
            }
            break;
        }

        case MAVLINK_MSG_ID_WAYPOINT_REQUEST_LIST: {
            Serial.println("waypoint request list");
            sendText(SEVERITY_LOW,PSTR("waypoint request list"));

            // decode
            mavlink_waypoint_request_list_t packet;
            mavlink_msg_waypoint_request_list_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component))
                break;

            // Start sending waypoints
            mavlink_msg_waypoint_count_send(
                _channel,msg->sysid,
                msg->compid,
                _cmdCount + 1); // + _home

            _cmdTimeLastSent        = millis();
            _cmdTimeLastReceived    = millis();
            _sendingCmds            = true;
            _receivingCmds          = false;
            _cmdDestSysId      = msg->sysid;
            _cmdDestCompId     = msg->compid;
            break;
        }

        case MAVLINK_MSG_ID_WAYPOINT_REQUEST: {
            //sendText(SEVERITY_LOW,PSTR("waypoint request"));

            // Check if sending waypiont
            if (!_sendingCmds) break;

            // decode
            mavlink_waypoint_request_t packet;
            mavlink_msg_waypoint_request_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component)) break;

            // send waypoint
            //tell_command = get_wp_with_index(packet.seq); TODO implement

            // set frame of waypoint
            uint8_t frame;
            if (tell_command.options & _useRelativeAlt) {
                frame = MAV_FRAME_GLOBAL_RELATIVE_ALT; // reference frame
            } else {
                frame = MAV_FRAME_GLOBAL; // reference frame
            }

            float param1 = 0, param2 = 0 , param3 = 0, param4 = 0;

            // time that the mav should loiter in milliseconds
            uint8_t current = 0; // 1 (true), 0 (false)
            if (packet.seq == _cmdIndex) current = 1;
            uint8_t autocontinue = 1; // 1 (true), 0 (false)
            float x = 0, y = 0, z = 0;

            if (tell_command.id < MAV_CMD_NAV_LAST) {
                // command needs scaling
                x = tell_command.lat/1.0e7; // local (x), global (latitude)
                y = tell_command.lng/1.0e7; // local (y), global (longitude)
                if (tell_command.options & _useRelativeAlt) {
                    z = (tell_command.alt - _home.alt) / 1.0e2; // because tell_command.alt already includes a += _home.alt
                } else {
                    z = tell_command.alt/1.0e2; // local (z), global/relative (altitude)
                }
            }


            switch (tell_command.id) {                    // Switch to map APM command fields inot MAVLink command fields
            case MAV_CMD_NAV_LOITER_TURNS:
            case MAV_CMD_NAV_TAKEOFF:
            case MAV_CMD_CONDITION_CHANGE_ALT:
            case MAV_CMD_DO_SET_HOME:
                param1 = tell_command.p1;
                break;

            case MAV_CMD_NAV_LOITER_TIME:
                param1 = tell_command.p1*10;    // APM loiter time is in ten second increments
                break;

            case MAV_CMD_CONDITION_DELAY:
            case MAV_CMD_CONDITION_DISTANCE:
                param1 = tell_command.lat;
                break;

            case MAV_CMD_DO_JUMP:
                param2 = tell_command.lat;
                param1 = tell_command.p1;
                break;

            case MAV_CMD_DO_REPEAT_SERVO:
                param4 = tell_command.lng;
            case MAV_CMD_DO_REPEAT_RELAY:
            case MAV_CMD_DO_CHANGE_SPEED:
                param3 = tell_command.lat;
                param2 = tell_command.alt;
                param1 = tell_command.p1;
                break;

            case MAV_CMD_DO_SET_PARAMETER:
            case MAV_CMD_DO_SET_RELAY:
            case MAV_CMD_DO_SET_SERVO:
                param2 = tell_command.alt;
                param1 = tell_command.p1;
                break;
            }

            mavlink_msg_waypoint_send(_channel,msg->sysid,
                                      msg->compid,packet.seq,frame,tell_command.id,current,autocontinue,
                                      param1,param2,param3,param4,x,y,z);

            // update last waypoint comm stamp
            _cmdTimeLastSent = millis();
            break;
        }

        case MAVLINK_MSG_ID_WAYPOINT_ACK: {
            //sendText(SEVERITY_LOW,PSTR("waypoint ack"));

            // decode
            mavlink_waypoint_ack_t packet;
            mavlink_msg_waypoint_ack_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component)) break;

            // check for error
            uint8_t type = packet.type; // ok (0), error(1)

            // turn off waypoint send
            _sendingCmds = false;
            break;
        }

        case MAVLINK_MSG_ID_PARAM_REQUEST_LIST: {
            Serial.println("param request list");
            sendText(SEVERITY_LOW,PSTR("param request list"));

            // decode
            mavlink_param_request_list_t packet;
            mavlink_msg_param_request_list_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component)) break;

            // Start sending parameters - next call to ::update will kick the first one out

            _queuedParameter = AP_Var::first();
            _queuedParameterIndex = 0;
            break;
        }

        case MAVLINK_MSG_ID_WAYPOINT_CLEAR_ALL: {
            //sendText(SEVERITY_LOW,PSTR("waypoint clear all"));

            // decode
            mavlink_waypoint_clear_all_t packet;
            mavlink_msg_waypoint_clear_all_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component)) break;

            // clear all waypoints
            uint8_t type = 0; // ok (0), error(1)
            _cmdCount.set_and_save(0);

            // send acknowledgement 3 times to makes sure it is received
            for (int i=0; i<3; i++) mavlink_msg_waypoint_ack_send(_channel,msg->sysid,msg->compid,type);

            break;
        }

        case MAVLINK_MSG_ID_WAYPOINT_SET_CURRENT: {
            //sendText(SEVERITY_LOW,PSTR("waypoint set current"));

            // decode
            mavlink_waypoint_set_current_t packet;
            mavlink_msg_waypoint_set_current_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component)) break;

            // set current waypoint
            _cmdIndex.set_and_save(packet.seq);

            {
                Location temp;  // XXX this is gross
                // TODO implement
                //temp = get_wp_with_index(packet.seq);
                //set_next_WP(&temp);
            }

            mavlink_msg_waypoint_current_send(_channel, _cmdIndex);
            break;
        }

        case MAVLINK_MSG_ID_WAYPOINT_COUNT: {
            //sendText(SEVERITY_LOW,PSTR("waypoint count"));

            // decode
            mavlink_waypoint_count_t packet;
            mavlink_msg_waypoint_count_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component)) break;

            // start waypoint receiving
            if (packet.count > _cmdMax) {
                packet.count = _cmdMax;
            }
            _cmdCount.set_and_save(packet.count - 1);
            _cmdTimeLastReceived = millis();
            _receivingCmds   = true;
            _sendingCmds     = false;
            _cmdRequestIndex   = 0;
            break;
        }

        case MAVLINK_MSG_ID_WAYPOINT: {
            // Check if receiving waypiont
            if (!_receivingCmds) break;

            // decode
            mavlink_waypoint_t packet;
            mavlink_msg_waypoint_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component)) break;

            // check if this is the requested waypoint
            if (packet.seq != _cmdRequestIndex) break;

            // store waypoint
            uint8_t loadAction = 0; // 0 insert in list, 1 exec now

            // defaults
            tell_command.id = packet.command;

            switch (packet.frame) {
            case MAV_FRAME_MISSION:
            case MAV_FRAME_GLOBAL: {
                tell_command.lat = 1.0e7*packet.x; // in as DD converted to * t7
                tell_command.lng = 1.0e7*packet.y; // in as DD converted to * t7
                tell_command.alt = packet.z*1.0e2; // in as m converted to cm
                tell_command.options = 0;
                break;
            }

            case MAV_FRAME_LOCAL: { // local (relative to _home position)
                tell_command.lat = 1.0e7*ToDeg(packet.x/
                                               (_radiusEarth*cos(ToRad(_home.lat/1.0e7)))) + _home.lat;
                tell_command.lng = 1.0e7*ToDeg(packet.y/_radiusEarth) + _home.lng;
                tell_command.alt = packet.z*1.0e2;
                tell_command.options = 1;
                break;
            }
            case MAV_FRAME_GLOBAL_RELATIVE_ALT: { // absolute lat/lng, relative altitude
                tell_command.lat = 1.0e7*packet.x; // in as DD converted to * t7
                tell_command.lng = 1.0e7*packet.y; // in as DD converted to * t7
                tell_command.alt = packet.z*1.0e2;
                tell_command.options = 1;
                break;
            }
            }

            switch (tell_command.id) {                    // Switch to map APM command fields inot MAVLink command fields
            case MAV_CMD_NAV_LOITER_TURNS:
            case MAV_CMD_NAV_TAKEOFF:
            case MAV_CMD_DO_SET_HOME:
                tell_command.p1 = packet.param1;
                break;

            case MAV_CMD_CONDITION_CHANGE_ALT:
                tell_command.p1 = packet.param1 * 100;
                break;

            case MAV_CMD_NAV_LOITER_TIME:
                tell_command.p1 = packet.param1 / 10;    // APM loiter time is in ten second increments
                break;

            case MAV_CMD_CONDITION_DELAY:
            case MAV_CMD_CONDITION_DISTANCE:
                tell_command.lat = packet.param1;
                break;

            case MAV_CMD_DO_JUMP:
                tell_command.lat = packet.param2;
                tell_command.p1 = packet.param1;
                break;

            case MAV_CMD_DO_REPEAT_SERVO:
                tell_command.lng = packet.param4;
            case MAV_CMD_DO_REPEAT_RELAY:
            case MAV_CMD_DO_CHANGE_SPEED:
                tell_command.lat = packet.param3;
                tell_command.alt = packet.param2;
                tell_command.p1 = packet.param1;
                break;

            case MAV_CMD_DO_SET_PARAMETER:
            case MAV_CMD_DO_SET_RELAY:
            case MAV_CMD_DO_SET_SERVO:
                tell_command.alt = packet.param2;
                tell_command.p1 = packet.param1;
                break;
            }

            // save waypoint - and prevent re-setting home
            if (packet.seq != 0)
                // TODO: implement
                //set_wp_with_index(tell_command, packet.seq);

                // update waypoint receiving state machine
                _cmdTimeLastReceived = millis();
            _cmdRequestIndex++;

            if (_cmdRequestIndex > _cmdCount) {
                uint8_t type = 0; // ok (0), error(1)

                mavlink_msg_waypoint_ack_send(
                    _channel,
                    msg->sysid,
                    msg->compid,
                    type);

                sendText(SEVERITY_LOW,PSTR("flight plan received"));
                _receivingCmds = false;
                // XXX ignores waypoint radius for individual waypoints, can
                // only set WP_RADIUS parameter
            }
            break;
        }

        case MAVLINK_MSG_ID_PARAM_SET: {
            AP_Var                  *vp;
            AP_Meta_class::Type_id  var_type;

            // decode
            mavlink_param_set_t packet;
            mavlink_msg_param_set_decode(msg, &packet);
            if (_checkTarget(packet.target_system,packet.target_component))
                break;

            // set parameter

            char key[_paramNameLengthMax+1];
            strncpy(key, (char *)packet.param_id, _paramNameLengthMax);
            key[_paramNameLengthMax] = 0;

            // find the requested parameter
            vp = AP_Var::find(key);
            if ((NULL != vp) &&                             // exists
                    !isnan(packet.param_value) &&               // not nan
                    !isinf(packet.param_value)) {               // not inf

                // add a small amount before casting parameter values
                // from float to integer to avoid truncating to the
                // next lower integer value.
                const float rounding_addition = 0.01;

                // fetch the variable type ID
                var_type = vp->meta_type_id();

                // handle variables with standard type IDs
                if (var_type == AP_Var::k_typeid_float) {
                    ((AP_Float *)vp)->set_and_save(packet.param_value);

                } else if (var_type == AP_Var::k_typeid_float16) {
                    ((AP_Float16 *)vp)->set_and_save(packet.param_value);

                } else if (var_type == AP_Var::k_typeid_int32) {
                    ((AP_Int32 *)vp)->set_and_save(packet.param_value+rounding_addition);

                } else if (var_type == AP_Var::k_typeid_int16) {
                    ((AP_Int16 *)vp)->set_and_save(packet.param_value+rounding_addition);

                } else if (var_type == AP_Var::k_typeid_int8) {
                    ((AP_Int8 *)vp)->set_and_save(packet.param_value+rounding_addition);
                } else {
                    // we don't support mavlink set on this parameter
                    break;
                }

                // Report back the new value if we accepted the change
                // we send the value we actually set, which could be
                // different from the value sent, in case someone sent
                // a fractional value to an integer type
                mavlink_msg_param_value_send(
                    _channel,
                    (int8_t *)key,
                    vp->cast_to_float(),
                    _countParameters(),
                    -1); // XXX we don't actually know what its index is...
            }

            break;
        } // end case




        }
    }

    uint16_t _countParameters() {
        // if we haven't cached the parameter count yet...
        if (0 == _parameterCount) {
            AP_Var  *vp;

            vp = AP_Var::first();
            do {
                // if a parameter responds to cast_to_float then we are going to be able to report it
                if (!isnan(vp->cast_to_float())) {
                    _parameterCount++;
                }
            } while (NULL != (vp = vp->next()));
        }
        return _parameterCount;
    }

    AP_Var * _findParameter(uint16_t index) {
        AP_Var  *vp;

        vp = AP_Var::first();
        while (NULL != vp) {

            // if the parameter is reportable
            if (!(isnan(vp->cast_to_float()))) {
                // if we have counted down to the index we want
                if (0 == index) {
                    // return the parameter
                    return vp;
                }
                // count off this parameter, as it is reportable but not
                // the one we want
                index--;
            }
            // and move to the next parameter
            vp = vp->next();
        }
        return NULL;
    }


    // check the target
    uint8_t _checkTarget(uint8_t sysid, uint8_t compid) {
        //Serial.print("target = "); Serial.print(sysid, DEC); Serial.print("\tcomp = "); Serial.println(compid, DEC);
        if (sysid != mavlink_system.sysid) {
            return 1;

        } else if(compid != mavlink_system.compid) {
            sendText(SEVERITY_LOW,PSTR("component id mismatch"));
            return 0; // XXX currently not receiving correct compid from gcs

        } else {
            return 0; // no error
        }
    }

};

uint8_t MavlinkComm::_nChannels = 0;
uint8_t MavlinkComm::_paramNameLengthMax = 13;
float MavlinkComm::_radiusEarth = 6378100;

} // namespace apo

#endif // AP_CommLink_H

// vim:ts=4:sw=4:tw=78:expandtab
