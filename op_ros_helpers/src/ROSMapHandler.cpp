/// \file  ROSMapHandler.h
/// \brief Handle road network mapping functionality for OpenPlanner, such as message subscription, reading the map, measurement conversion
/// 		It handles (Vector Maps, KML, Lanelet2)
/// \author Hatem Darweesh
/// \date June 18, 2021

#include "op_ros_helpers/ROSMapHandler.h"
#include "op_ros_helpers/op_ROSHelpers.h"
#include "op_planner/MappingHelpers.h"
#include "op_planner/KmlMapLoader.h"
#include "op_planner/Lanelet2MapLoader.h"
#include "op_planner/VectorMapLoader.h"

namespace PlannerHNS
{

MapHandler::MapHandler()
{
	m_MapType = PlannerHNS::MAP_KML_FILE;
	m_bMap = false;
	m_bMapBinReceived = false;
	m_bKmlMapFileNameReceived = false;
}

MapHandler::~MapHandler()
{

}

bool MapHandler::IsMapLoaded()
{
	return m_bMap;
}

void MapHandler::InitMapHandler(ros::NodeHandle& nh, const std::string& source_topic, const std::string& map_path_topic, const std::string& map_origin_topic)
{
	std::string str_map_path;
	nh.getParam(map_path_topic , m_MapPath);
	int iSource = 0;
	nh.getParam(source_topic, iSource);
	std::string str_origin;
	nh.getParam(map_origin_topic , str_origin);

	if(iSource == 0)
	{
		m_MapType = PlannerHNS::MAP_AUTOWARE;
		sub_bin_map = nh.subscribe("lanelet_map_bin", 1, &MapHandler::callbackGetLanelet2, this);
	}
	else if (iSource == 1)
	{
		m_MapType = PlannerHNS::MAP_FOLDER;
	}
	else if(iSource == 2)
	{
		m_MapType = PlannerHNS::MAP_KML_FILE;
	}
	else if(iSource == 3)
	{
		m_MapType = PlannerHNS::MAP_LANELET_2;
		std::cout << "This should be it:" << str_origin << "\n";
		std::vector<std::string> lat_lon_alt = PlannerHNS::MappingHelpers::SplitString(str_origin, ",");
		for(auto stringer : lat_lon_alt)
		{
			std::cout << stringer;
		}
		std::cout << "\n";
		if(lat_lon_alt.size() == 3)
		{
			m_MapOrigin.pos.lat = atof(lat_lon_alt.at(0).c_str());
			m_MapOrigin.pos.lon = atof(lat_lon_alt.at(1).c_str());
			m_MapOrigin.pos.alt = 0.0;
		}
		std::cout << lat_lon_alt.size() << "\n";
	}
	else if(iSource == 4)
	{
		m_MapType = PlannerHNS::MAP_KML_FILE_NAME;
	}

	std::cout << "Autoware format is here\n";
}

void MapHandler::LoadMap(RoadNetwork& map, bool bEnableLaneChange)
{
	if(m_bMap) return;

	map.Clear();

	if(m_MapType == PlannerHNS::MAP_KML_FILE)
	{
		LoadKmlMap(m_MapPath, map);
	}
	else if(m_MapType == PlannerHNS::MAP_KML_FILE_NAME && m_bKmlMapFileNameReceived)
	{
		LoadKmlMap(m_MapFileName, map);
	}
	else if (m_MapType == PlannerHNS::MAP_FOLDER)
	{
		PlannerHNS::VectorMapLoader vec_loader(1, bEnableLaneChange);
		vec_loader.LoadFromFile(m_MapPath, map);
	}
	else if (m_MapType == PlannerHNS::MAP_LANELET_2)
	{
		map.origin = m_MapOrigin;
		PlannerHNS::Lanelet2MapLoader map_loader;
		if(m_MapPath.size() > 0)
		{
			map_loader.LoadMap(m_MapPath, map);
		}
		else if(m_bMapBinReceived)
		{
			map_loader.LoadMap(m_Lanelet2Bin, map);
		}
	}
	else if (m_MapType == PlannerHNS::MAP_AUTOWARE)
	{
		if(m_MapRaw.AreMessagesReceived())
		{
			PlannerHNS::VectorMapLoader vec_loader(1, bEnableLaneChange);
			vec_loader.LoadFromData(m_MapRaw, map);
		}
	}

	if(map.roadSegments.size() > 0)
	{
		PlannerHNS::MappingHelpers::ConvertVelocityToMeterPerSecond(map);
		m_bMap = true;
		m_bKmlMapFileNameReceived = false;
		m_bMapBinReceived = false;
		std::cout << " ******* Map Is Loaded successfully from the tracker." << std::endl;
	}
}

void MapHandler::LoadKmlMap(const std::string& file_name, RoadNetwork& map)
{
	PlannerHNS::KmlMapLoader kml_loader;
	kml_loader.LoadKML(file_name, map);
}

void MapHandler::callbackGetkmlMapFileName(const std_msgs::String& file_name)
{
	m_MapFileName = file_name.data;
	m_bKmlMapFileNameReceived = true;
}

void MapHandler::callbackGetLanelet2(autoware_lanelet2_msgs::MapBin msg)
{
	std::cout << "Get in there Lanetlet map\n";
	m_Lanelet2Bin = msg;
	m_bMapBinReceived = true;
}

void MapHandler::callbackGetVMLanes(const vector_map_msgs::LaneArray& msg)
{
	std::cout << "Received Lanes" << std::endl;
	if(m_MapRaw.pLanes == nullptr)
		m_MapRaw.pLanes = new UtilityHNS::AisanLanesFileReader(msg);
}

void MapHandler::callbackGetVMPoints(const vector_map_msgs::PointArray& msg)
{
	std::cout << "Received Points" << std::endl;
	if(m_MapRaw.pPoints  == nullptr)
		m_MapRaw.pPoints = new UtilityHNS::AisanPointsFileReader(msg);
}

void MapHandler::callbackGetVMdtLanes(const vector_map_msgs::DTLaneArray& msg)
{
	std::cout << "Received dtLanes" << std::endl;
	if(m_MapRaw.pCenterLines == nullptr)
		m_MapRaw.pCenterLines = new UtilityHNS::AisanCenterLinesFileReader(msg);
}

void MapHandler::callbackGetVMIntersections(const vector_map_msgs::CrossRoadArray& msg)
{
	std::cout << "Received CrossRoads" << std::endl;
	if(m_MapRaw.pIntersections == nullptr)
		m_MapRaw.pIntersections = new UtilityHNS::AisanIntersectionFileReader(msg);
}

void MapHandler::callbackGetVMAreas(const vector_map_msgs::AreaArray& msg)
{
	std::cout << "Received Areas" << std::endl;
	if(m_MapRaw.pAreas == nullptr)
		m_MapRaw.pAreas = new UtilityHNS::AisanAreasFileReader(msg);
}

void MapHandler::callbackGetVMLines(const vector_map_msgs::LineArray& msg)
{
	std::cout << "Received Lines" << std::endl;
	if(m_MapRaw.pLines == nullptr)
		m_MapRaw.pLines = new UtilityHNS::AisanLinesFileReader(msg);
}

void MapHandler::callbackGetVMStopLines(const vector_map_msgs::StopLineArray& msg)
{
	std::cout << "Received StopLines" << std::endl;
	if(m_MapRaw.pStopLines == nullptr)
		m_MapRaw.pStopLines = new UtilityHNS::AisanStopLineFileReader(msg);
}

void MapHandler::callbackGetVMSignal(const vector_map_msgs::SignalArray& msg)
{
	std::cout << "Received Signals" << std::endl;
	if(m_MapRaw.pSignals  == nullptr)
		m_MapRaw.pSignals = new UtilityHNS::AisanSignalFileReader(msg);
}

void MapHandler::callbackGetVMVectors(const vector_map_msgs::VectorArray& msg)
{
	std::cout << "Received Vectors" << std::endl;
	if(m_MapRaw.pVectors  == nullptr)
		m_MapRaw.pVectors = new UtilityHNS::AisanVectorFileReader(msg);
}

void MapHandler::callbackGetVMCurbs(const vector_map_msgs::CurbArray& msg)
{
	std::cout << "Received Curbs" << std::endl;
	if(m_MapRaw.pCurbs == nullptr)
		m_MapRaw.pCurbs = new UtilityHNS::AisanCurbFileReader(msg);
}

void MapHandler::callbackGetVMRoadEdges(const vector_map_msgs::RoadEdgeArray& msg)
{
	std::cout << "Received Edges" << std::endl;
	if(m_MapRaw.pRoadedges  == nullptr)
		m_MapRaw.pRoadedges = new UtilityHNS::AisanRoadEdgeFileReader(msg);
}

void MapHandler::callbackGetVMWayAreas(const vector_map_msgs::WayAreaArray& msg)
{
	std::cout << "Received Way areas" << std::endl;
	if(m_MapRaw.pWayAreas  == nullptr)
		m_MapRaw.pWayAreas = new UtilityHNS::AisanWayareaFileReader(msg);
}

void MapHandler::callbackGetVMCrossWalks(const vector_map_msgs::CrossWalkArray& msg)
{
	std::cout << "Received CrossWalks" << std::endl;
	if(m_MapRaw.pCrossWalks == nullptr)
		m_MapRaw.pCrossWalks = new UtilityHNS::AisanCrossWalkFileReader(msg);
}

void MapHandler::callbackGetVMNodes(const vector_map_msgs::NodeArray& msg)
{
	std::cout << "Received Nodes" << std::endl;
	if(m_MapRaw.pNodes == nullptr)
		m_MapRaw.pNodes = new UtilityHNS::AisanNodesFileReader(msg);
}


} /* namespace PlannerHNS */
