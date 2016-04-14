/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2015, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */

/*
App : srba-slam
Info: A front-end for Relative Bundle Adjustment (RBA). The program is
      flexible to cope with several types of sensors and problem parameterizations.
	  Input datasets are plain text, in the format generated by RWT [1].
	  More docs on SRBA in [2].

	  [1] http://www.mrpt.org/Application:srba-slam
	  [2] http://www.mrpt.org/srba
*/

#include "srba-slam_common.h"
//#include <mrpt/otherlibs/tclap/CmdLine.h>

using namespace mrpt;
using namespace srba;
using namespace std;

// It's a singleton:
RBA_implemented_registry & RBA_implemented_registry::getInstance()
{
	static RBA_implemented_registry inst;
	return inst;
}

void RBA_implemented_registry::doRegister(
	factory_functor_t functor,
	const std::string &description)
{
	m_registered.push_back( TEntry(functor,description) );
}

void RBA_implemented_registry::dumpAllKnownProblems() const
{
	cout << "Implemented RBA problem types:\n";
	for (list_t::const_iterator it=m_registered.begin();it!=m_registered.end();++it)
		cout << " " << it->description << endl;
}

RBA_Run_BasePtr RBA_implemented_registry::searchImplementation( RBASLAM_Params &params) const
{
	for (list_t::const_iterator it=m_registered.begin();it!=m_registered.end();++it)
	{
		RBA_Run_BasePtr ptr = (*(it->functor))(params);
		if (ptr.get()!=NULL) return ptr;
	}

	return RBA_Run_BasePtr();
}

// ---------------- All the parameters of this app: --------------

// Parse all cmd-line arguments at construction
// ---------------------------------------------------
RBASLAM_Params::RBASLAM_Params(int argc, char**argv) :
	cmd (argv[0], ' ', mrpt::system::MRPT_getVersion().c_str()),
	arg_dataset("d","dataset","Dataset file (e.g. 'dataset1_SENSOR.txt', etc.)",false,"","",cmd),
	arg_gt_map("","gt-map","Ground-truth landmark map file (e.g. 'dataset1_GT_MAP.txt', etc.)",false,"","",cmd),
	arg_gt_path("","gt-path","Ground-truth robot path file (e.g. 'dataset1_GT_PATH.txt', etc.)",false,"","",cmd),
	arg_max_known_feats_per_frame("","max-fixed-feats-per-kf","Create fixed & known-location features",false,0,"",cmd),
	arg_se2("","se2","Relative poses are SE(2)",cmd, false),
	arg_se3("","se3","Relative poses are SE(3)",cmd, false),
	arg_lm2d("","lm-2d","Relative landmarks are Euclidean 2D points",cmd, false),
	arg_lm3d("","lm-3d","Relative landmarks are Euclidean 3D points",cmd, false),
	arg_graph_slam("","graph-slam","Define a relative graph-slam problem (no landmarks)",cmd, false),
	arg_obs("","obs","Type of observations in the dataset (use --list-problems to see available types)",false,"","",cmd),
	arg_sensor_params("","sensor-params-cfg-file","Config file from where to load the sensor parameters",false,"","",cmd),
	arg_list_obs("","list-problems","List all implemented values for '--obs'",cmd, false),
	arg_no_gui("","no-gui","Don't show the live gui",cmd, false),
	arg_gui_step_by_step("","step-by-step","If showing the gui, go step by step",cmd, false),
	arg_profile_stats("","profile-stats","Generate profile stats to CSV files, with the given prefix",false,"","stats",cmd),
	arg_profile_stats_length("","profile-stats-length","Length in KFs of each saved profiled segment",false,10,"",cmd),
	arg_add_noise("","add-noise","Add AWG noise to the dataset",cmd, false),
	arg_noise("","noise","One sigma of the noise model of every component of observations (images,...) or to linear components if they're mixed (default: sensor-dependent)\n If a SRBA config is provided, it will override this value.",false,0.0,"noise_std",cmd),
	arg_noise_ang("","noise-ang","One sigma of the noise model of every angular component of observations, in degrees (default: sensor-dependent)\n If a SRBA config is provided, it will override this value.",false,0.0,"noise_std",cmd),
	arg_max_tree_depth("","max-spanning-tree-depth","Overrides this parameter in config files",false,4,"depth",cmd),
	arg_max_opt_depth("","max-optimize-depth","Overrides this parameter in config files",false,4,"depth",cmd),
	arg_max_lambda("","max-lambda","Marq-Lev. optimization: maximum lambda to stop iterating",false,1e20,"depth",cmd),
	arg_max_iters("","max-iters","Max. number of optimization iterations.",false,20,"",cmd),
	arg_submap_size("","submap-size","Number of KFs in each 'submap' of the arc-creation policy.",false,20,"20",cmd),
	arg_verbose("v","verbose","0:quiet, 1:informative, 2:tons of info",false,1,"",cmd),
	arg_random_seed("","random-seed","<0: randomize; >=0, use this random seed.",false,-1,"",cmd),
	arg_rba_params_cfg_file("","cfg-file-rba","Config file (*.cfg) for the RBA parameters",false,"","rba.cfg",cmd),
	arg_write_rba_params_cfg_file("","cfg-file-rba-bootstrap","Writes an empty config file (*.cfg) for the RBA parameters and exit.",false,"","rba.cfg",cmd),
	arg_video("","create-video","Creates a video with the animated GUI output (*.avi).",false,"","out.avi",cmd),
	arg_gui_delay("","gui-delay","Milliseconds of delay between GUI frames. Default:0. Increase for correctly generating videos, etc.",false,0,"",cmd),
	arg_video_fps("","video-fps","If creating a video, its FPS (Hz).",false,30.0,"",cmd),
	arg_debug_dump_cur_spantree("","debug-dump-cur-spantree","Dump to files the current spanning tree",cmd, false),
	arg_save_final_graph("","save-final-graph","Save the final graph-map of KFs to a .dot file",false,"","final-map.dot",cmd),
	arg_save_final_graph_landmarks("","save-final-graph-landmarks","Save the final graph-map (all KFs and all Landmarks) to a .dot file",false,"","final-map.dot",cmd),
	arg_eval_overall_sqr_error("","eval-overall-sqr-error","At end, evaluate the overall square error for all the observations with the final estimated model",cmd, false),
	arg_eval_overall_se3_error("","eval-overall-se3-error","At end, evaluate the overall SE3 error for all relative poses",cmd, false),
	arg_eval_connectivity("","eval-connectivity","At end, make stats on the graph connectivity",cmd, false)
{
	// Parse arguments:
	if (!cmd.parse( argc, argv ))
		throw std::runtime_error(""); // should exit, but without any error msg (should have been dumped to cerr)

	if (arg_list_obs.isSet())
	{
		RBA_implemented_registry & inst = RBA_implemented_registry::getInstance();
		inst.dumpAllKnownProblems();
		throw std::runtime_error(""); // Exit
	}

	if (!arg_obs.isSet() && !arg_graph_slam.isSet())
		throw std::runtime_error("Error: argument --obs is mandatory (in non-graph-SLAM) to select the type of observations.\nRun with --list-problems or --help to see all the options or visit http://www.mrpt.org/srba for docs and examples.\n");

	if (arg_obs.isSet() && arg_graph_slam.isSet())
		throw std::runtime_error("Error: argument --obs doesn't apply to relative graph-SLAM.\nRun with --list-problems or --help to see all the options or visit http://www.mrpt.org/srba for docs and examples.\n");

	if ( (arg_se2.isSet() && arg_se3.isSet()) ||
		 (!arg_se2.isSet() && !arg_se3.isSet()) )
		 throw std::runtime_error("Exactly one of --se2 or --se3 flags must be set.");

	if ( (!arg_graph_slam.isSet() && ( (arg_lm2d.isSet() && arg_lm3d.isSet()) || (!arg_lm2d.isSet() && !arg_lm3d.isSet()) ) ) ||
		 (arg_graph_slam.isSet() && (arg_lm2d.isSet() || arg_lm3d.isSet()) ) )
		 throw std::runtime_error("Exactly one of --lm-2d or --lm-3d or --arg_graph_slam flags must be set.");
}

int main(int argc, char**argv)
{
	try
	{
		// Parse command-line arguments:
		// ---------------------------------
		RBASLAM_Params  config(argc,argv);

		// Construct the RBA problem object:
		// -----------------------------------
		RBA_implemented_registry &inst = RBA_implemented_registry::getInstance();
		// Search in the registry of all implemented instances:
		RBA_Run_BasePtr rba = inst.searchImplementation(config);

		if (rba.get()==NULL)
			throw std::runtime_error("Sorry: the given combination of pose, landmark and sensor models wasn't precompiled in this program!\nRun with '--list-problems' to see available options.\n");

		// Run:
		return rba->run(config);
	}
	catch (std::exception &e)
	{
		const std::string sErr = std::string(e.what());
		if (!sErr.empty())
			std::cerr << "*EXCEPTION*:\n" << sErr << std::endl;
		return 1;
	}
}

