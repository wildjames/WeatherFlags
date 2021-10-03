import requests

print("Lets go")

OWM_url = "https://api.openweathermap.org/data/2.5/onecall?lat=53.383&lon=-1.4659&exclude=daily,alerts,minutely&appid=0158ecae23f7262be48cf43492c32596&units=metric"

unix_url = "https://showcase.api.linx.twenty57.net/UnixTime/fromunixtimestamp?unixtimestamp="

resp = requests.get(OWM_url)
data = resp.json()

hourly_data = data['hourly']

for hour in hourly_data:
    print("I have weather data for time: {}".format(hour['dt']))
    # time_resp = requests.get(unix_url+str(hour['dt']))
    # time_data = time_resp.json()
    # actual_timestring = time_data['Datetime']

    print("I have the following data:")
    print(hour.keys())
    print("Temperature feels like: {}".format(hour["feels_like"]))
    print("The 'weather' field says: {}".format(hour["weather"]))
    if "rain" in hour.keys():
        print("The 'rain' field says: {}".format(hour["rain"]))

    print("\n\n")