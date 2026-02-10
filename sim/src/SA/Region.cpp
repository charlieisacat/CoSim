// Created by Francisco Munoz Martinez on 02/07/2019

#include "SA/Region.h"
#include "SA/utility.h"

Region::Region(unsigned int R, unsigned int S, unsigned int C, unsigned int K, unsigned int G, unsigned int N, unsigned int X, unsigned int Y)
{
    this->R = R;
    this->S = S;
    this->C = C / G; // The user has to specify this parameter in terms of the whole feature map
    this->K = K / G; // Idem
    this->G = G;
    this->N = N;
    this->X = X;
    this->Y = Y;
}

void Region::printConfiguration(std::ofstream &out, unsigned int indent)
{
    out << ind(indent) << "\"RegionConfiguration\" : {" << std::endl;
    out << ind(indent + IND_SIZE) << "\"R\" : " << this->R << "," << std::endl;
    out << ind(indent + IND_SIZE) << "\"S\" : " << this->S << "," << std::endl;
    out << ind(indent + IND_SIZE) << "\"C\" : " << this->C << "," << std::endl;
    out << ind(indent + IND_SIZE) << "\"K\" : " << this->K << "," << std::endl;
    out << ind(indent + IND_SIZE) << "\"G\" : " << this->G << "," << std::endl;
    out << ind(indent + IND_SIZE) << "\"N\" : " << this->N << "," << std::endl;
    out << ind(indent + IND_SIZE) << "\"X\" : " << this->X << "," << std::endl;
    out << ind(indent + IND_SIZE) << "\"Y\" : " << this->Y << "," << std::endl;
    // out << ind(indent+IND_SIZE) << "\"X_\" : " << this->X_ << "," << std::endl;
    // out << ind(indent+IND_SIZE) << "\"Y_\" : " << this->Y_  << std::endl;
    out << ind(indent) << "}";
}
