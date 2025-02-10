--application.lua
-- D05, GPIO14
pin = 5
last_state = 1
gpio.mode(pin, gpio.INPUT, gpio.PULLUP)

c = nil

function worker()
    state = gpio.read(pin)
    --print("worker: state ", state)
    if state ~= last_state then
        --print("worker: new state ", state)
		last_state = state
        if c ~= nil then
            print("worker: publish new state ", state)
            c:publish("aquarium/level", state, 0, 0)
        end
    end    
end

mqtt_connect_event = function(client)
  print("Connected to mqtt")
  c = client 
end

mqtt_connfail_event = function(client, reason)
  print("Failed to connect to mqtt. Reason: " .. reason)
  c = nil
end

mqtt_puback_event = function(client)
  print("Publised to mqtt")
end

m = mqtt.Client("aquarium_level_sensor", 1)
m:on("connect", mqtt_connect_event)
m:on("connfail", mqtt_connfail_event)
m:on("puback", mqtt_puback_event)

m:connect(MQTT_HOST, MQTT_PORT, false)

tmr.create():alarm(1000, tmr.ALARM_AUTO, worker)
