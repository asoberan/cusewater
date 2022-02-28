import folium
import branca
import requests
import flask
import pandas as pd

from flask import request
from folium.plugins import FastMarkerCluster
from rapidfuzz import process

URL = "https://opendata.arcgis.com/datasets/e1deb6e9e4b74071af272982d8f9994e_0.geojson"
CNY=(43.0481221, -76.1474244)
LEGEND = {
    'LEAD': 'green',
    'CAST IRON': 'blue',
    'COPPER': 'red',
    'GAL.IRON': 'orange',
    'DUCTILE': 'black',
    'OTHER': 'black',
}

app = flask.Flask(__name__)

def get_water_services():
    try:
        response = requests.get(URL)
        data = response.json()
        data_df = pd.json_normalize(data["features"])
        data_df = data_df.drop("properties.BNAME", axis=1)
        data_df = data_df.drop("geometry.type", axis=1)
        data_df["properties.TAP_ADDRESS"] = data_df["properties.TAP_ADDRESS"].apply(lambda x: " ".join(x.lower().split()))
        data_df[["y", "x"]] = pd.DataFrame(data_df["geometry.coordinates"].tolist(), index=data_df.index) 
        data_df = data_df.drop("geometry.coordinates", axis=1)
        
        return data_df
    
    except Exception as e:
        print(e)
        
        return None

def create_marker_cluster(water_df):

    callback = ('function (row) {'
                    'var colors = {'
                    "'LEAD': 'green',"
                    "'CAST IRON': 'blue',"
                    "'COPPER': 'red',"
                    "'GAL.IRON': 'orange',"
                    "'DUCTILE': 'black',"
                    "'OTHER': 'black',"
                    '};'

                    'var marker = L.marker(new L.LatLng(row[0], row[1]), {color: "red"});'
                    'var icon = L.AwesomeMarkers.icon({'
                    "icon: 'info-sign',"
                    "markerColor: colors[row[3].trim()],"
                    "prefix: 'glyphicon',"
                    "extraClasses: 'fa-rotate-0'"
                    '});'

                    'marker.setIcon(icon);'
                    
                    "marker.bindTooltip('Serviced: ' + row[2]);"
                    
                    'return marker};'
    )   


    fast_marker_cluster = FastMarkerCluster(water_df[["x", "y", "properties.SERV_INSTALL", "properties.PTYPE"]].values, callback=callback, disable_clustering_at_zoom=18, spiderfyOnMaxZoom=False)

    return fast_marker_cluster

def create_map(marker_cluster, location=CNY, zoom_start=10):
   map_new = folium.Map(location=location, max_zoom=19, zoom_start=zoom_start)
   map_new.add_child(marker_cluster)

   return map_new

@app.route("/map/", methods=["GET", "POST"])
def map_index(address=None):
    water_df = get_water_services()
    marker_cluster = create_marker_cluster(water_df)
    
    if water_df is not None:    
                
        fig = branca.element.Figure(height='100vh')
        
        if request.method == "POST":
            details = request.form
            address = details["address"].lower()
            
            if address != "":
                address_index = process.extractOne(address, water_df["properties.TAP_ADDRESS"])[2]
                address_coords = water_df[["x", "y"]].values[address_index]
                address_serv_date = water_df["properties.SERV_INSTALL"][address_index]
                address_ptype = water_df["properties.PTYPE"][address_index].strip()

                address_marker = folium.Marker(location=tuple(address_coords), icon=folium.Icon(color=LEGEND[address_ptype], icon="home"), tooltip=f'Serviced: {address_serv_date}')

                marker_cluster.add_child(address_marker)

                map_new = create_map(marker_cluster, location=tuple(address_coords), zoom_start=19)
    
                map_new.add_to(fig)

            return flask.render_template("map/index.html", folium_map=fig._repr_html_())

        map_new = create_map(marker_cluster)

        fig = branca.element.Figure(height='100vh')
    
        map_new.add_to(fig)

        return flask.render_template("map/index.html", folium_map=fig._repr_html_())

    else:
        print("Could not get water services information.")

@app.route("/", methods=["GET", "POST"])
def index():
    return flask.render_template("index.html")

@app.route("/about/", methods=["GET"])
def about_index():
    return flask.render_template("about/index.html")

if __name__ == "__main__":
    app.run()
