#include "sim.h"

// Read a python.numpy style range, but it is a floating point range
// thus, blank resolves to 0 or big_pos

void tw::input::PythonRange(std::string& source,tw::Float *v0,tw::Float *v1)
{
	size_t colonPos;
	std::string sub;
	colonPos = source.find(':');

	if (colonPos>0)
	{
		sub = source.substr(0,colonPos);
		*v0 = std::stod(sub,NULL);
	}
	else
		*v0 = 0.0;

	if (source.length() > colonPos+1)
	{
		sub = source.substr(colonPos+1,source.length()-colonPos-1);
		*v1 = std::stod(sub,NULL);
	}
	else
		*v1 = tw::big_pos;
}

// Find species or source with "name" and add a new profile to its list.  Return the profile.

Profile* tw::input::GetProfile(Grid* theGrid,const std::string& name,const std::string& profileType)
{
	tw::Int i;
	Profile* theProfile;

	theProfile = NULL;

	for (i=0;i<theGrid->module.size();i++)
		if (theGrid->module[i]->name==name)
		{
			if (profileType=="uniform")
				theGrid->module[i]->profile.push_back(new UniformProfile(theGrid->clippingRegion));
			if (profileType=="piecewise")
				theGrid->module[i]->profile.push_back(new PiecewiseProfile(theGrid->clippingRegion));
			if (profileType=="channel")
				theGrid->module[i]->profile.push_back(new ChannelProfile(theGrid->clippingRegion));
			if (profileType=="column")
				theGrid->module[i]->profile.push_back(new ColumnProfile(theGrid->clippingRegion));
			if (profileType=="beam" || profileType=="gaussian")
				theGrid->module[i]->profile.push_back(new GaussianProfile(theGrid->clippingRegion));
			if (profileType=="corrugated")
				theGrid->module[i]->profile.push_back(new CorrugatedProfile(theGrid->clippingRegion));
			theProfile = theGrid->module[i]->profile.back();
		}

	return theProfile;
}

// Read boundary conditions

tw_boundary_spec tw::input::ConvertBoundaryString(std::string& theString)
{
	if (theString=="periodic" || theString=="cyclic")
		return cyclic;
	if (theString=="reflective" || theString=="reflecting")
		return reflecting;
	if (theString=="absorbing" || theString=="open")
		return absorbing;
	if (theString=="emitting" || theString=="emissive")
		return emitting;
	if (theString=="axisymmetric" || theString=="axisymmetry")
		return axisymmetric;

	return cyclic;
}

void tw::input::ReadBoundaryTerm(tw_boundary_spec *low,tw_boundary_spec *high,std::stringstream& theString,std::string& command)
{
	std::string word;
	tw::Int axis = 0;
	if (command=="xboundary") axis = 1;
	if (command=="yboundary") axis = 2;
	if (command=="zboundary") axis = 3;
	if (axis>0)
	{
		theString >> word >> word;
		low[axis] = ConvertBoundaryString(word);
		theString >> word;
		high[axis] = ConvertBoundaryString(word);
	}
}

void tw::input::NormalizeInput(const UnitConverter& uc,std::string& in_out)
{
	// To be used in preprocessing input file
	// un-normalized quantities are signaled with percent,e.g. %1.0m
	// signals to replace the string with normalized length corresponding to 1 meter
	std::size_t endpos;
	std::string units;
	std::stringstream temp;
	tw::Float qty,nqty=tw::small_pos;
	if (in_out[0]=='%')
	{
		try { qty = std::stod(in_out.substr(1),&endpos); }
		catch (std::invalid_argument) { throw tw::FatalError("Invalid unit conversion macro : " + in_out); }
		units = in_out.substr(1+endpos);

		// Length Conversions
		if (units=="um")
			nqty = uc.MKSToSim(length_dim,1e-6*qty);
		if (units=="mm")
			nqty = uc.MKSToSim(length_dim,1e-3*qty);
		if (units=="cm")
			nqty = uc.MKSToSim(length_dim,1e-2*qty);
		if (units=="m")
			nqty = uc.MKSToSim(length_dim,qty);

		// Time Conversions
		if (units=="fs")
			nqty = uc.MKSToSim(time_dim,1e-15*qty);
		if (units=="ps")
			nqty = uc.MKSToSim(time_dim,1e-12*qty);
		if (units=="ns")
			nqty = uc.MKSToSim(time_dim,1e-9*qty);
		if (units=="us")
			nqty = uc.MKSToSim(time_dim,1e-6*qty);
		if (units=="s")
			nqty = uc.MKSToSim(time_dim,qty);

		// Number Density Conversions
		if (units=="m-3")
			nqty = uc.MKSToSim(density_dim,qty);
		if (units=="cm-3")
			nqty = uc.CGSToSim(density_dim,qty);

		// Energy Density Conversions
		if (units=="Jm3")
			nqty = uc.MKSToSim(energy_density_dim,qty);
		if (units=="Jcm3")
			nqty = uc.MKSToSim(energy_density_dim,1e6*qty);

		// Temperature Conversions
		if (units=="eV")
			nqty = uc.eV_to_sim(qty);
		if (units=="K")
			nqty = uc.MKSToSim(temperature_dim,qty);

		if (nqty==tw::small_pos)
			throw tw::FatalError("Unrecognized Units " + in_out);
		else
		{
			temp << nqty; // don't use std::to_string; it rounds small numbers to 0.
			in_out = temp.str();
		}
	}
}
