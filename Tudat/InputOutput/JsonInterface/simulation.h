/*    Copyright (c) 2010-2017, Delft University of Technology
 *    All rigths reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 */

#ifndef TUDAT_JSONINTERFACE_SIMULATION_H
#define TUDAT_JSONINTERFACE_SIMULATION_H

#include <Tudat/SimulationSetup/tudatSimulationHeader.h>

#include "Support/deserialization.h"
#include "Support/valueAccess.h"
#include "Support/valueConversions.h"

#include "Environment/spice.h"
#include "Environment/body.h"
#include "Propagation/propagator.h"
#include "Mathematics/integrator.h"
#include "Propagation/export.h"
#include "Support/options.h"

namespace tudat
{

namespace json_interface
{

//! Class for managing JSON-based simulations.
template< typename TimeType = double, typename StateScalarType = double >
class JsonSimulationManager
{
public:
    bool profiling = false;

    //! Constructor.
    /*!
     * Constructor.
     * \param initialClockTime Initial clock time from which the cummulative CPU time during the propagation will be
     * computed. Default is the moment at which the constructor was called.
     */
    JsonSimulationManager( const std::chrono::steady_clock::time_point initialClockTime = std::chrono::steady_clock::now( ) )
        : initialClockTime_( initialClockTime ) { }

    //! Reset the root JSON input file.
    /*!
     * Reset the root JSON input file.
     * \param inputFile Path to the root JSON input file. Can be absolute or relative (to the working directory).
     */
    void setUpFromJSONFile( const std::string& inputFile )
    {
        inputFilePath_ = getPathForJSONFile( inputFile );
        boost::filesystem::current_path( inputFilePath_.parent_path( ) );

        setUpFromJSONObject( getDeserializedJSON( inputFilePath_ ) );
    }

    //! Reset the `json` object.
    /*!
     * Reset the `json` object.
     * \param jsonObject The new `json` object to be used for creating the simulation settings.
     */
    void setUpFromJSONObject( const nlohmann::json& jsonObject )
    {
        jsonObject_ = jsonObject;
        originalJsonObject_ = jsonObject_;

        // std::cout << jsonObject_.dump( 2 ) << std::endl;
        // throw;

        // Clear global variable keeping track of the keys that have been accessed
        clearAccessHistory( );

        if ( profiling )
        {
            std::cout << "parse: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }

        updateSettingsFromJSONObject( );
    }

    //! Run the simulation.
    /*!
     * @copybrief run
     * <br/>
     * Before running the simulation, the JSON representation of `this` will be exported if requested in
     * applicationOptions_, and a message will be printed if requested in applicationOptions_.
     * <br/>
     * If some of the keys in jsonObject_ haven't been used, a message may be printed or an error may be thrown
     * depending on applicationOptions_.
     * <br/>
     * After running the simulation, a message will be printed if requested in applicationOptions_.
     */
    virtual void run( )
    {
        // Check if any keys in jsonObject_ haven't been used
        checkUnusedKeys( jsonObject_, applicationOptions_->unusedKey_ );

        if ( profiling )
        {
            std::cout << "checkUnusedKeys: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }

        // Export full settings JSON file if requested
        if ( ! applicationOptions_->fullSettingsFile_.empty( ) )
        {
            exportAsJSON( applicationOptions_->fullSettingsFile_ );
        }

        // Print message on propagation start if requested
        if ( applicationOptions_->notifyOnPropagationStart_ )
        {
            std::cout << "Propagation of file " << inputFilePath_ << " started." << std::endl;
        }

        // Run simulation
        dynamicsSimulator_->integrateEquationsOfMotion( propagatorSettings_->getInitialStates( ) );

        // Print message on propagation termination if requested
        if ( applicationOptions_->notifyOnPropagationTermination_ )
        {
            if ( dynamicsSimulator_->integrationCompletedSuccessfully( ) )
            {
                std::cout << "SUCCESS: propagation of file " << inputFilePath_ << " terminated with no errors."
                          << std::endl;
            }
            else
            {
                std::cout << "FAILURE: propagation of file " << inputFilePath_ << " terminated with errors."
                          << std::endl;
            }
        }

        if ( profiling )
        {
            std::cout << "run: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Export the results of the dynamics simulation according to the export settings.
    /*!
     * @copybrief exportResults
     */
    virtual void exportResults( )
    {
        if ( applicationOptions_->tagOutputFilesIfPropagationFails_ &&
             ! dynamicsSimulator_->integrationCompletedSuccessfully( ) )
        {
            // Add header "FAILURE" to output files
            for ( boost::shared_ptr< ExportSettings >& exportSettings : exportSettingsVector_ )
            {
                exportSettings->header_ = "FAILURE\n" + exportSettings->header_;
            }
        }

        exportResultsOfDynamicsSimulator( dynamicsSimulator_, exportSettingsVector_ );

        if ( profiling )
        {
            std::cout << "exportResults: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Export `this` as a `json` object.
    /*!
     * @copybrief getAsJSON
     * \return JSON representation of `this`.
     */
    nlohmann::json getAsJSON( )
    {
        updateJSONObjectFromSettings( );
        return jsonObject_;
    }

    //! Export `this` as a `json` object to the file \p exportPath.
    /*!
     * @copybrief exportAsJSON
     * \param exportPath Path to which the contents of the JSON object are to be exported.
     * \param tabSize Size of tabulations in the exported file (default = 2, i.e. 2 spaces). If set to 0, no
     * indentantion or line breaks will be used, so the exported file will contain just one line.
     */
    void exportAsJSON( const path& exportPath, const unsigned int tabSize = 2 )
    {
        if ( ! boost::filesystem::exists( exportPath.parent_path( ) ) )
        {
            boost::filesystem::create_directories( exportPath.parent_path( ) );
        }
        std::ofstream outputFile( exportPath.string( ) );
        outputFile << getAsJSON( ).dump( tabSize );
        outputFile.close( );
    }

    //! Get original JSON object (defined at construction or last time setInputFile was called).
    /*!
     * @copybrief getOriginalJSONObject
     * \remark The returned JSON object is the result of combining all the JSON files that
     * may be included in the root JSON file into one single object containing all the settings, but before default
     * values have been loaded or unit conversions have been applied.
     * \return The original JSON object.
     */
    nlohmann::json getOriginalJSONObject( ) const
    {
        return originalJsonObject_;
    }


    //! Get simulation start epoch.
    TimeType getStartEpoch( ) const
    {
        return integratorSettings_->initialTime_;
    }

    //! Get maximum simulation end epoch. Returns `TUDAT_NAN` if there is no time termination condition.
    TimeType getEndEpoch( ) const
    {
        return getTerminationEpoch< TimeType >( propagatorSettings_ );
    }

    //! Get integrator settings.
    boost::shared_ptr< numerical_integrators::IntegratorSettings< TimeType > > getIntegratorSettings( ) const
    {
        return integratorSettings_;
    }

    //! Get Spice settings (NULL if Spice is not used).
    boost::shared_ptr< SpiceSettings > getSpiceSettings( ) const
    {
        return spiceSettings_;
    }

    //! Get global frame origin.
    std::string getGlobalFrameOrigin( ) const
    {
        return globalFrameOrigin_;
    }

    //! Get global frame orientation.
    std::string getGlobalFrameOrientation( ) const
    {
        return globalFrameOrientation_;
    }

    //! Get map of body settings.
    std::map< std::string, boost::shared_ptr< simulation_setup::BodySettings > > getBodySettingsMap( ) const
    {
        return bodySettingsMap_;
    }

    //! Get body map.
    simulation_setup::NamedBodyMap getBodyMap( ) const
    {
        return bodyMap_;
    }

    //! Get body named \p bodyName.
    boost::shared_ptr< simulation_setup::Body > getBody( const std::string& bodyName ) const
    {
        return bodyMap_.at( bodyName );
    }

    //! Get propagation settings.
    boost::shared_ptr< propagators::MultiTypePropagatorSettings< StateScalarType > > getPropagatorSettings( ) const
    {
        return propagatorSettings_;
    }

    //! Get vector of export settings (each element corresponds to an output file).
    std::vector< boost::shared_ptr< ExportSettings > > getExportSettingsVector( ) const
    {
        return exportSettingsVector_;
    }

    //! Get application options.
    boost::shared_ptr< ApplicationOptions > getApplicationOptions( ) const
    {
        return applicationOptions_;
    }

    //! Get dynamics simulator.
    boost::shared_ptr< propagators::SingleArcDynamicsSimulator< StateScalarType, TimeType > > getDynamicsSimulator( ) const
    {
        return dynamicsSimulator_;
    }


protected:

    //! Reset integratorSettings_ from the current jsonObject_.
    /*!
     * @copybrief resetIntegratorSettings
     */
    virtual void resetIntegratorSettings( )
    {
        updateFromJSON( integratorSettings_, jsonObject_, Keys::integrator );

        if ( profiling )
        {
            std::cout << "resetIntegratorSettings: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Reset spiceSettings_ from the current jsonObject_.
    /*!
     * @copybrief resetSpice
     * Loads the requested kernels in Tudat (if any).
     */
    virtual void resetSpice( )
    {
        spiceSettings_ = NULL;
        updateFromJSONIfDefined( spiceSettings_, jsonObject_, Keys::spice );
        loadSpiceKernels( spiceSettings_ );

        if ( profiling )
        {
            std::cout << "resetSpice: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Reset bodySettingsMap_ and bodyMap_ from the current jsonObject_.
    /*!
     * @copybrief resetBodies
     */
    virtual void resetBodies( )
    {
        globalFrameOrigin_ = getValue< std::string >( jsonObject_, Keys::globalFrameOrigin, "SSB" );
        globalFrameOrientation_ = getValue< std::string >( jsonObject_, Keys::globalFrameOrientation, "ECLIPJ2000" );
        updateBodiesFromJSON( jsonObject_, bodyMap_, bodySettingsMap_, globalFrameOrigin_, globalFrameOrientation_,
                              spiceSettings_, integratorSettings_ );

        if ( profiling )
        {
            std::cout << "resetBodies: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Reset exportSettingsVector_ from the current jsonObject_.
    /*!
     * @copybrief resetExportSettings
     */
    virtual void resetExportSettings( )
    {
        exportSettingsVector_.clear( );
        updateFromJSONIfDefined( exportSettingsVector_, jsonObject_, Keys::xport );

        if ( profiling )
        {
            std::cout << "resetExportSettings: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Reset propagatorSettings_ from the current jsonObject_
    /*!
     * @copybrief resetPropagatorSettings
     * Tries to infer the initial states from the body ephemeris if not provided.
     * Creates the integrated state models using bodyMap_
     */
    virtual void resetPropagatorSettings( )
    {
        // Update jsonObject_ by determining initial states if not provided directly to the propagator settings:
        // * By obtaining the initial states from body properties (and transforming to Cartesian if necessary)
        // * By infering initial states from body ephemeris
        determineInitialStates< TimeType, StateScalarType >( jsonObject_, bodyMap_, integratorSettings_ );

        if ( profiling )
        {
            std::cout << "resetPropagatorSettings@determineInitialStates: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }

        // Update propagatorSettings_ from jsonObject_
        updateFromJSON( propagatorSettings_, jsonObject_ );

        if ( profiling )
        {
            std::cout << "resetPropagatorSettings@updateFromJSON: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }

        // Create integrated state models (acceleration, mass-rate, rotational models)
        propagatorSettings_->resetIntegratedStateModels( bodyMap_ );

        if ( profiling )
        {
            std::cout << "resetPropagatorSettings@resetIntegratedStateModels: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }

        // Update dependent variables to save
        resetDependentVariableSaveSettings( propagatorSettings_, exportSettingsVector_ );

        if ( profiling )
        {
            std::cout << "resetPropagatorSettings@resetDependentVariableSaveSettings: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Reset applicationOptions_ from the current jsonObject_.
    /*!
     * @copybrief resetApplicationOptions
     */
    virtual void resetApplicationOptions( )
    {
        applicationOptions_ = boost::make_shared< ApplicationOptions >( );
        updateFromJSONIfDefined( applicationOptions_, jsonObject_, Keys::options );

        if ( profiling )
        {
            std::cout << "resetApplicationOptions: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }

    //! Reset dynamicsSimulator_ for the current bodyMap_, integratorSettings_ and propagatorSettings_.
    /*!
     * @copybrief resetDynamicsSimulator
     */
    virtual void resetDynamicsSimulator( )
    {
        dynamicsSimulator_ =
                boost::make_shared< propagators::SingleArcDynamicsSimulator< StateScalarType, TimeType > >(
                    bodyMap_, integratorSettings_, propagatorSettings_, false, false, false, initialClockTime_ );

        if ( profiling )
        {
            std::cout << "resetDynamicsSimulator: " << std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::steady_clock::now( ) - initialClockTime_ ).count( ) * 1.0e-3 << " s" << std::endl;
            initialClockTime_ = std::chrono::steady_clock::now( );
        }
    }


    //! Integrator settings.
    boost::shared_ptr< numerical_integrators::IntegratorSettings< TimeType > > integratorSettings_;

    //! Spice settings (NULL if Spice is not used).
    boost::shared_ptr< SpiceSettings > spiceSettings_;

    //! Global frame origin.
    std::string globalFrameOrigin_;

    //! Global frame orientation.
    std::string globalFrameOrientation_;

    //! Map of body settings.
    std::map< std::string, boost::shared_ptr< simulation_setup::BodySettings > > bodySettingsMap_;

    //! Body map.
    simulation_setup::NamedBodyMap bodyMap_;

    //! Propagation settings.
    boost::shared_ptr< propagators::MultiTypePropagatorSettings< StateScalarType > > propagatorSettings_;

    //! Vector of export settings (each element corresponds to an output file).
    std::vector< boost::shared_ptr< ExportSettings > > exportSettingsVector_;

    //! Application options.
    boost::shared_ptr< ApplicationOptions > applicationOptions_;

    //! Dynamics simulator.
    boost::shared_ptr< propagators::SingleArcDynamicsSimulator< StateScalarType, TimeType > > dynamicsSimulator_;


private:

    //! Update the JSON object with all the data from the current settings (objests).
    void updateJSONObjectFromSettings( )
    {
        jsonObject_ = boost::make_shared< JsonSimulationManager< TimeType, StateScalarType > >( *this );
    }

    //! Update all the settings (objects) from the JSON object.
    virtual void updateSettingsFromJSONObject( )
    {
        resetIntegratorSettings( );  // must be called before resetSpice, resetBodies, resetPropagatorSettings
        resetSpice( );
        resetBodies( );
        resetExportSettings( );      // must be called before resetPropagatorSettings
        resetPropagatorSettings( );
        resetApplicationOptions( );
        resetDynamicsSimulator( );
    }

    //! Absolute path to the input file.
    std::chrono::steady_clock::time_point initialClockTime_;

    //! Absolute path to the input file.
    path inputFilePath_;

    //! Original JSON object with all the settings read directly from the input file.
    nlohmann::json originalJsonObject_;

    //! JSON object with the current settings.
    nlohmann::json jsonObject_;

};


//! Function to create a `json` object from a Simulation object.
template< typename TimeType, typename StateScalarType >
void to_json( nlohmann::json& jsonObject,
              const boost::shared_ptr< JsonSimulationManager< TimeType, StateScalarType > >& jsonSimulationManager )
{
    if ( ! jsonSimulationManager )
    {
        return;
    }

    // integrator
    jsonObject[ Keys::integrator ] = jsonSimulationManager->getIntegratorSettings( );

    // spice
    assignIfNotNull( jsonObject, Keys::spice, jsonSimulationManager->getSpiceSettings( ) );

    // bodies
    jsonObject[ Keys::globalFrameOrigin ] = jsonSimulationManager->getGlobalFrameOrigin( );
    jsonObject[ Keys::globalFrameOrientation ] = jsonSimulationManager->getGlobalFrameOrientation( );
    jsonObject[ Keys::bodies ] = jsonSimulationManager->getBodySettingsMap( );

    // export
    assignIfNotEmpty( jsonObject, Keys::xport, jsonSimulationManager->getExportSettingsVector( ) );

    // options
    jsonObject[ Keys::options ] = jsonSimulationManager->getApplicationOptions( );

    // propagation + termination + options.printInterval
    propagators::to_json( jsonObject, jsonSimulationManager->getPropagatorSettings( ) );
}

} // namespace json_interface

} // namespace tudat

#endif // TUDAT_JSONINTERFACE_SIMULATION_H