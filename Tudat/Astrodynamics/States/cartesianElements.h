/*    Copyright (c) 2010-2012 Delft University of Technology.
 *
 *    This software is protected by national and international copyright.
 *    Any unauthorized use, reproduction or modification is unlawful and
 *    will be prosecuted. Commercial and non-private application of the
 *    software in any form is strictly prohibited unless otherwise granted
 *    by the authors.
 *
 *    The code is provided without any warranty; without even the implied
 *    warranty of merchantibility or fitness for a particular purpose.
 *
 *    Changelog
 *      YYMMDD    Author            Comment
 *      101020    K. Kumar          First creation of code.
 *      101026    K. Kumar          Added position and velocity vectors, modified existing
 *                                  comments.
 *      101201    E. Iorfida        Modified punctuation.
 *      101202    J. Melman         Corrected some Doxygen comments, changed Cartesian into
 *                                  Cartesian. Private variables are obsolete now, since everything
 *                                  is done with state_ from base class.
 *      110110    K. Kumar          Minor modifications.
 *      110131    K. Kumar          Moved code to CartesianPositionElements  class; added
 *                                  inheritance.
 *      110204    K. Kumar          Removed "vector" from naming.
 *
 *    References
 *
 */

#ifndef TUDAT_CARTESIAN_ELEMENTS_H
#define TUDAT_CARTESIAN_ELEMENTS_H

#include <iostream>

#include "Tudat/Astrodynamics/States/cartesianPositionElements.h"

namespace tudat
{
namespace astrodynamics
{
namespace states
{

//! Cartesian elements class.
/*!
 * Cartesian elements class.
 */
class CartesianElements : public CartesianPositionElements
{
public:

    //! Default constructor.
    /*!
     * Default constructor.
     */
    CartesianElements( ) : position_( Eigen::Vector3d::Zero( ) ) { state.setZero( 6 ); }

    //! Set Cartesian element: xDot.
    /*!
     * Sets the Cartesian element: xDot.
     * \param cartesianElementXDot Cartesian element: xDot.
     */
    void setCartesianElementXDot( const double cartesianElementXDot )
    {
        state( 3 ) = cartesianElementXDot;
    }

    //! Set Cartesian element: yDot.
    /*!
     * Sets the Cartesian element: yDot.
     * \param cartesianElementYDot Cartesian element: yDot.
     */
    void setCartesianElementYDot( const double cartesianElementYDot )
    {
        state( 4 ) = cartesianElementYDot;
    }

    //! Set Cartesian element: zDot.
    /*!
     * Sets the Cartesian element: zDot.
     * \param cartesianElementZDot Cartesian element: zDot.
     */
    void setCartesianElementZDot( const double cartesianElementZDot )
    {
        state( 5 ) = cartesianElementZDot;
    }

    //! Set position.
    /*!
     * Sets the position.
     * \return position Position.
     */
    void setPosition( const Eigen::Vector3d& position ) { state.segment( 0, 3 ) = position; }

    //! Set velocity.
    /*!
     * Sets the velocity.
     * \return velocity Velocity.
     */
    void setVelocity( const Eigen::Vector3d& velocity ) { state.segment( 3, 3 ) = velocity; }

    //! Get Cartesian element: xDot.
    /*!
     * Returns the Cartesian element: xDot.
     * \return Cartesian element: xDot.
     */
    double getCartesianElementXDot( ) { return state( 3 ); }

    //! Get Cartesian element: yDot.
    /*!
     * Returns the Cartesian element: yDot.
     * \return Cartesian element: yDot.
     */
    double getCartesianElementYDot( ) { return state( 4 ); }

    //! Get Cartesian element: zDot.
    /*!
     * Returns the Cartesian element: zDot.
     * \return Cartesian element: zDot.
     */
    double getCartesianElementZDot( ) { return state( 5 ); }

    //! Get position.
    /*!
     * Returns the position.
     * \return Position.
     */
    Eigen::Vector3d getPosition( ) { position_ = state.segment( 0, 3 ); return position_; }

    //! Get velocity.
    /*!
     * Returns the velocity.
     * \return Velocity.
     */
    Eigen::Vector3d getVelocity( ) { velocity_ = state.segment( 3, 3 ); return velocity_; }

    //! Overload ostream to print class information.
    /*!
     * Overloads ostream to print class information.
     * \param stream Stream object.
     * \param cartesianElements CartesianElements object.
     * \return Stream object.
     */
    friend std::ostream& operator<<( std::ostream& stream, CartesianElements& cartesianElements )
    {
        stream << "The state is set to: " << cartesianElements.state << std::endl;
        return stream;
    }

protected:

private:

    //! Position.
    /*!
     * Position.
     */
    Eigen::Vector3d position_;

    //! Velocity.
    /*!
     * Velocity.
     */
    Eigen::Vector3d velocity_;
};

} // namespace states
} // namespace astrodynamics
} // namespace tudat

#endif // TUDAT_CARTESIAN_ELEMENTS_H