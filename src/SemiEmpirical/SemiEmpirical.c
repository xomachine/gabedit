/* SemiEmpirical.c */
/**********************************************************************************************************
Copyright (c) 2002-2013 Abdul-Rahman Allouche. All rights reserved

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the Gabedit), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions
  of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
************************************************************************************************************/
#include "../../Config.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#include "../Common/Global.h"
#include "../Utils/AtomsProp.h"
#include "../Utils/Utils.h"
#include "../Utils/Constants.h"
#include "../Geometry/Fragments.h"
#include "../Geometry/DrawGeom.h"
#include "AtomSE.h"
#include "MoleculeSE.h"
#include "SemiEmpiricalModel.h"
#include "SemiEmpirical.h"

static void calculateGradientMopac(SemiEmpiricalModel* seModel);
static void calculateEnergyMopac(SemiEmpiricalModel* seModel);
static void calculateGradientFireFly(SemiEmpiricalModel* seModel);
static void calculateEnergyFireFly(SemiEmpiricalModel* seModel);


/****************************************************************/
static void getMultiplicityName(gint multiplicity, gchar* buffer)
{
	if(multiplicity==1) sprintf(buffer,"Singlet");
	else if(multiplicity==2) sprintf(buffer,"Doublet");
	else if(multiplicity==3) sprintf(buffer,"Triplet");
	else if(multiplicity==4) sprintf(buffer,"Quartet");
	else if(multiplicity==5) sprintf(buffer,"Quintet");
	else if(multiplicity==6) sprintf(buffer,"Sextet");
	else sprintf(buffer,"UNKNOWN");
}
/*****************************************************************************/
static gboolean getEnergyMopac(gchar* fileNameOut, gdouble* energy)
{
	FILE* file = NULL;
	gchar buffer[1024];
	gchar* pdest = NULL;

 	file = FOpen(fileNameOut, "rb");
	if(!file) return FALSE;
	 while(!feof(file))
	 {
		if(!fgets(buffer,BSIZE,file))break;
		pdest = strstr( buffer, " FINAL HEAT OF FORMATION");
		if(pdest) 
		{
			pdest = strstr( buffer, "=");
			if(pdest)
			{
				if(sscanf(pdest+1,"%lf",energy)==1)
				{
					fclose(file);
					return TRUE;
				}
			}
		}
	 }
	fclose(file);
	return FALSE;
}
/*****************************************************************************/
static gboolean getGradientMopac(gchar* fileNameOut, SemiEmpiricalModel *seModel)
{
	FILE* file = NULL;
	gchar buffer[1024];
	gchar* pdest = NULL;
	gboolean Ok = FALSE;
	gdouble tmp;
	gint i;
	gint j;

 	file = FOpen(fileNameOut, "rb");
	if(!file) return FALSE;
	 while(!feof(file))
	 {
		if(!fgets(buffer,BSIZE,file))break;
		pdest = strstr( buffer, "PARAMETER     ATOM    TYPE            VALUE       GRADIENT");
		if(pdest) 
		{
			for(i=0;i<seModel->molecule.nAtoms;i++)
			for(j=0;j<3;j++)
			{
				if(!fgets(buffer,BSIZE,file))break;
				pdest = strstr( buffer, "CARTESIAN");
				if(pdest)
				{
					if(sscanf(pdest+12,"%lf %lf",&tmp,&seModel->molecule.gradient[j][i])!=2)
					{
						fclose(file);
						return FALSE;
					}
				}
			}
			Ok = TRUE;
			break;
	 	}
		pdest = strstr( buffer, "Cartesian Gradients"); /* MOZYME Keyword */
		if(pdest) 
		{
			gchar td[100];
			gint d;
			if(!fgets(buffer,BSIZE,file))break; /*Atom       X  ....*/
			if(!fgets(buffer,BSIZE,file))break; /* backspace */
			for(i=0;i<seModel->molecule.nAtoms;i++)
			{
				if(!fgets(buffer,BSIZE,file)) /* 1  O    0.000   -4.566    0.027  */
				{
					fclose(file);
					return FALSE;
				}
				if(sscanf(buffer,"%d %s %lf %lf %lf",&d, td, 
						&seModel->molecule.gradient[0][i],
						&seModel->molecule.gradient[1][i],
						&seModel->molecule.gradient[2][i]
					 )
						!=5)
					{
						fclose(file);
						return FALSE;
					}
			}
			Ok = TRUE;
			break;
	 	}
	 }
	fclose(file);
	return Ok;
}
/*****************************************************************************/
static gchar* runOneMopac(SemiEmpiricalModel* seModel, gchar* keyWords)
{
	FILE* file = NULL;
	FILE* fileSH = NULL;
	gint j;
	gchar* fileNameIn = NULL;
	gchar* fileNameOut = NULL;
	gchar* fileNameSH = NULL;
	gchar multiplicityStr[100];
	gchar buffer[1024];
	MoleculeSE m = seModel->molecule;
#ifdef G_OS_WIN32
	gchar c='%';
#endif

	if(m.nAtoms<1) return fileNameOut;
#ifndef G_OS_WIN32
	fileNameSH = g_strdup_printf("%s%sMopacOne.sh",seModel->workDir,G_DIR_SEPARATOR_S);
#else
	fileNameSH = g_strdup_printf("%s%sMopacOne.bat",seModel->workDir,G_DIR_SEPARATOR_S);
#endif
 	fileSH = FOpen(fileNameSH, "w");
	if(!fileSH) return FALSE;
#ifdef G_OS_WIN32
	fprintf(fileSH,"@echo off\n");
	fprintf(fileSH,"set PATH=%cPATH%c;\"%s\"\n",c,c,mopacDirectory);
#endif

	getMultiplicityName(seModel->molecule.spinMultiplicity, multiplicityStr);

	fileNameIn = g_strdup_printf("%s%sOne.mop",seModel->workDir,G_DIR_SEPARATOR_S);
 	file = FOpen(fileNameIn, "w");
	if(!file) 
	{
 		if(fileNameIn) g_free(fileNameIn);
 		if(fileNameOut) g_free(fileNameOut);
 		if(fileNameSH) g_free(fileNameSH);
		return FALSE;
	}
	fprintf(file,"* ===============================\n");
	fprintf(file,"* Input file for Mopac\n");
	fprintf(file,"* ===============================\n");
	if(seModel->molecule.spinMultiplicity>1)
		fprintf(file,"%s UHF CHARGE=%d %s\n",keyWords,seModel->molecule.totalCharge,multiplicityStr);
	else
		fprintf(file,"%s CHARGE=%d %s\n",keyWords,seModel->molecule.totalCharge,multiplicityStr);
	fprintf(file,"\n");
	fprintf(file,"Mopac file generated by Gabedit\n");

	for(j=0;j<m.nAtoms;j++)
	{
	fprintf(file," %s %f %d %f %d %f %d\n", 
			m.atoms[j].prop.symbol,
			m.atoms[j].coordinates[0],
			1,
			m.atoms[j].coordinates[1],
			1,
			m.atoms[j].coordinates[2],
			1
			);
	}
	fclose(file);
#ifndef G_OS_WIN32
	fprintf(fileSH,"%s %s\n",NameCommandMopac,fileNameIn);
	fclose(fileSH);
	sprintf(buffer,"chmod u+x %s",fileNameSH);
	{int ierr = system(buffer);}
	{ int ierr = system(fileNameSH);}
#else
	fprintf(fileSH,"\"%s\" \"%s\"\n",NameCommandMopac,fileNameIn);
	fclose(fileSH);
	sprintf(buffer,"\"%s\"",fileNameSH);
	{int ierr = system(buffer);}
#endif

	unlink(fileNameIn);
	unlink(fileNameSH);
 	if(fileNameIn) g_free(fileNameIn);
 	if(fileNameSH) g_free(fileNameSH);
	fileNameOut = g_strdup_printf("%s%sOne.out",seModel->workDir,G_DIR_SEPARATOR_S);
	return fileNameOut;
}
/**********************************************************************/
static SemiEmpiricalModel newMopacModel(gchar* method, gchar* dirName, SemiEmpiricalModelConstraints constraints)
{
	SemiEmpiricalModel seModel = newSemiEmpiricalModel(method, dirName, constraints);

	seModel.klass->calculateGradient = calculateGradientMopac;
	seModel.klass->calculateEnergy = calculateEnergyMopac;

	return seModel;
}
/**********************************************************************/
static void calculateGradientMopac(SemiEmpiricalModel* seModel)
{
	gint i;
	gint j;
	MoleculeSE m = seModel->molecule;
	gchar* keyWords = NULL;
	gchar* fileOut = NULL;
	if(!seModel) return;
	if(seModel->molecule.nAtoms<1) return;
	if(!seModel->method) return;
	keyWords = g_strdup_printf("%s 1SCF GRAD",seModel->method);
	fileOut = runOneMopac(seModel, keyWords);

	if(fileOut)
	{
		for(j=0;j<3;j++)
			for( i=0; i<m.nAtoms;i++)
				m.gradient[j][i] = 0.0;
		if(!getGradientMopac(fileOut, seModel))
		{
			StopCalcul=TRUE;
			set_text_to_draw(_("Problem : I cannot caculate the Gradient... "));
			set_statubar_operation_str(_("Calculation Stopped "));
			drawGeom();
			gtk_widget_set_sensitive(StopButton, FALSE);
			Waiting(1);
			return;
		}
		getEnergyMopac(fileOut, &seModel->molecule.energy);
		g_free(fileOut);
	}

}
/**********************************************************************/
static void calculateEnergyMopac(SemiEmpiricalModel* seModel)
{
	gchar* keyWords = NULL;
	gchar* fileOut = NULL;
	if(!seModel) return;
	if(seModel->molecule.nAtoms<1) return;
	if(!seModel->method) return;
	keyWords = g_strdup_printf("%s 1SCF",seModel->method);
	fileOut = runOneMopac(seModel, keyWords);
	if(fileOut)
	{
		getEnergyMopac(fileOut, &seModel->molecule.energy);
		g_free(fileOut);
	}

}
/**********************************************************************/
SemiEmpiricalModel createMopacModel (GeomDef* geom,gint Natoms,gint charge, gint spin, gchar* method, gchar* dirName, SemiEmpiricalModelConstraints constraints)
{
	SemiEmpiricalModel seModel = newMopacModel(method, dirName, constraints);

	seModel.molecule = createMoleculeSE(geom,Natoms, charge, spin,TRUE);
	setRattleConstraintsParameters(&seModel);
	
	return seModel;
}
/**********************************************************************/
static gboolean getEnergyFireFly(gchar* fileNameOut, gdouble* energy)
{
	FILE* file = NULL;
	gchar buffer[1024];
	gchar* pdest = NULL;
	gboolean OK = FALSE;

 	file = FOpen(fileNameOut, "rb");
	if(!file) return FALSE;
	 while(!feof(file))
	 {
		if(!fgets(buffer,BSIZE,file))break;
		pdest = strstr( buffer, "HEAT OF FORMATION IS");
		if(pdest) 
		{
			pdest = strstr( buffer, "S");
			if(pdest)
			{
				if(sscanf(pdest+1,"%lf",energy)==1)
				{
					OK = TRUE;
					/* break;*/
				}
			}
		}
	 }
	fclose(file);
	return OK;
}
/*****************************************************************************/
static gboolean getGradientFireFly(gchar* fileNameOut, SemiEmpiricalModel *seModel)
{
	FILE* file = NULL;
	gchar buffer[1024];
	gchar stmp[1024];
	gchar* pdest = NULL;
	gboolean Ok = FALSE;
	gint itmp;
	gint i;
	gint j;

 	file = FOpen(fileNameOut, "rb");
	if(!file) return FALSE;
	 while(!feof(file))
	 {
		if(!fgets(buffer,BSIZE,file))break;
		pdest = strstr( buffer, "ATOM                 E'X               E'Y               E'Z");
		if(pdest) 
		{
			for(i=0;i<seModel->molecule.nAtoms;i++)
			{
				if(!fgets(buffer,BSIZE,file))break;
				if(sscanf(buffer,"%d %s %lf %lf %lf",&itmp, stmp,
							&seModel->molecule.gradient[0][i],
							&seModel->molecule.gradient[1][i],
							&seModel->molecule.gradient[2][i]
							)!=5)
				{
					fclose(file);
					return FALSE;
				}
				for(j=0;j<3;j++) seModel->molecule.gradient[j][i] *= 627.50944796/BOHR_TO_ANG;
			}
			Ok = TRUE;
			break;
	 	}
	 }
	fclose(file);
	return Ok;
}
/*****************************************************************************/
static gchar* runOneFireFly(SemiEmpiricalModel* seModel, gchar* keyWords)
{
	FILE* file = NULL;
	FILE* fileSH = NULL;
	gint j;
	gchar* fileNameIn = NULL;
	gchar* fileNameOut = NULL;
	gchar* fileNameSH = NULL;
	gchar multiplicityStr[100];
	gchar buffer[1024];
	MoleculeSE m = seModel->molecule;
	gchar* fileNamePrefix = NULL;
#ifdef G_OS_WIN32
	gchar c='%';
#endif

	if(m.nAtoms<1) return fileNameOut;
#ifndef G_OS_WIN32
	fileNameSH = g_strdup_printf("%s%sFireFlyOne.sh",seModel->workDir,G_DIR_SEPARATOR_S);
#else
	fileNameSH = g_strdup_printf("%s%sFireFlyOne.bat",seModel->workDir,G_DIR_SEPARATOR_S);
#endif
 	fileSH = FOpen(fileNameSH, "w");
	if(!fileSH) return FALSE;
#ifdef G_OS_WIN32
	fprintf(fileSH,"@echo off\n");
	fprintf(fileSH,"set PATH=%cPATH%c;\"%s\"\n",c,c,fireflyDirectory);
#endif

	getMultiplicityName(seModel->molecule.spinMultiplicity, multiplicityStr);

	fileNameIn = g_strdup_printf("%s%sOne.inp",seModel->workDir,G_DIR_SEPARATOR_S);
	fileNameOut = g_strdup_printf("%s%sOne.out",seModel->workDir,G_DIR_SEPARATOR_S);


 	file = FOpen(fileNameIn, "w");
	if(!file) 
	{
 		if(fileNameIn) g_free(fileNameIn);
 		if(fileNameOut) g_free(fileNameOut);
 		if(fileNameSH) g_free(fileNameSH);
		return FALSE;
	}
	fprintf(file,"! ======================================================\n");
	fprintf(file,"!  Input file for FireFly\n"); 
	fprintf(file,"! ======================================================\n");
	if(strstr(keyWords,"RUNTYP"))
	{
		sscanf(strstr(keyWords,"RUNTYP"),"%s",buffer);
		fprintf(file," $CONTRL %s $END\n",buffer);
	}
	if(strstr(keyWords,"SCFTYP"))
	{
		sscanf(strstr(keyWords,"SCFTYP"),"%s",buffer);
		fprintf(file," $CONTRL %s $END\n",buffer);
	}
	else
	{
		if(seModel->molecule.spinMultiplicity==1)
			fprintf(file," $CONTRL SCFTYP=RHF $END\n");
		else
			fprintf(file," $CONTRL SCFTYP=UHF $END\n");
	}

	fprintf(file," $CONTRL ICHARG=%d MULT=%d $END\n",seModel->molecule.totalCharge,seModel->molecule.spinMultiplicity);
	if(strstr(keyWords,"GBASIS"))
	{
		sscanf(strstr(keyWords,"GBASIS"),"%s",buffer);
		fprintf(file," $BASIS %s $END\n",buffer);
	}
	fprintf(file," $DATA\n");
	fprintf(file,"Molecule specification\n");
	fprintf(file,"C1\n");
	for(j=0;j<m.nAtoms;j++)
	{
		gchar* symbol = m.atoms[j].prop.symbol;
		SAtomsProp prop = prop_atom_get(symbol);
		fprintf(file,"%s %f %f %f %f\n", 
			symbol,
			(gdouble)prop.atomicNumber,
			m.atoms[j].coordinates[0],
			m.atoms[j].coordinates[1],
			m.atoms[j].coordinates[2]
			);
	}
	fprintf(file," $END\n");
	fclose(file);
	fileNamePrefix = g_strdup_printf("%s%sWorkFF",seModel->workDir,G_DIR_SEPARATOR_S);
#ifndef G_OS_WIN32
	if(!strcmp(NameCommandFireFly,"pcgamess") || !strcmp(NameCommandFireFly,"nohup pcgamess")
	|| !strcmp(NameCommandFireFly,"firefly") || !strcmp(NameCommandFireFly,"nohup firefly"))
	{
		fprintf(fileSH,"mkdir %stmp\n",fileNamePrefix);
		fprintf(fileSH,"cd %stmp\n",fileNamePrefix);
		fprintf(fileSH,"cp %s input\n",fileNameIn);
		fprintf(fileSH,"%s -p -o %s\n",NameCommandFireFly,fileNameOut);
		fprintf(fileSH,"cd ..\n");
		fprintf(fileSH,"rm PUNCH\n");
		fprintf(fileSH,"/bin/rm -r  %stmp\n",fileNamePrefix);
	}
	else
		fprintf(fileSH,"%s %s",NameCommandFireFly,fileNameIn);
#else
	 if(!strcmp(NameCommandFireFly,"pcgamess") ||
	 !strcmp(NameCommandFireFly,"firefly") )
	{
        	fprintf(fileSH,"mkdir \"%stmp\"\n",fileNamePrefix);
		addUnitDisk(fileSH, fileNamePrefix);
	 	fprintf(fileSH,"cd \"%stmp\"\n",fileNamePrefix);
         	fprintf(fileSH,"copy \"%s\" input\n",fileNameIn);
         	fprintf(fileSH,"%s -p -o \"%s\"\n",NameCommandFireFly,fileNameOut);
	 	fprintf(fileSH,"cd ..\n");
         	fprintf(fileSH,"del PUNCH 2>nul\n");
         	fprintf(fileSH,"del /Q  \"%stmp\"\n",fileNamePrefix);
         	fprintf(fileSH,"rmdir  \"%stmp\"\n",fileNamePrefix);
	}
	else
		fprintf(fileSH,"%s %s",NameCommandFireFly,fileNameIn);
#endif
	fclose(fileSH);
#ifndef G_OS_WIN32
	sprintf(buffer,"chmod u+x %s",fileNameSH);
	{int ierr = system(buffer);}
	{ int ierr = system(fileNameSH);}
#else
	sprintf(buffer,"\"%s\"",fileNameSH);
	{int ierr = system(buffer);}
#endif
	unlink(fileNameIn);
	unlink(fileNameSH);
 	if(fileNamePrefix) g_free(fileNamePrefix);
 	if(fileNameIn) g_free(fileNameIn);
 	if(fileNameSH) g_free(fileNameSH);
	return fileNameOut;
}
/**********************************************************************/
static SemiEmpiricalModel newFireFlyModel(gchar* method, gchar* dirName, SemiEmpiricalModelConstraints constraints)
{
	SemiEmpiricalModel seModel = newSemiEmpiricalModel(method, dirName, constraints);

	seModel.klass->calculateGradient = calculateGradientFireFly;
	seModel.klass->calculateEnergy = calculateEnergyFireFly;

	return seModel;
}
/**********************************************************************/
static void calculateGradientFireFly(SemiEmpiricalModel* seModel)
{
	gint i;
	gint j;
	MoleculeSE m = seModel->molecule;
	gchar* keyWords = NULL;
	gchar* fileOut = NULL;
	if(!seModel) return;
	if(seModel->molecule.nAtoms<1) return;
	if(!seModel->method) return;
	keyWords = g_strdup_printf("RUNTYP=GRADIENT GBASIS=%s",seModel->method);
	fileOut = runOneFireFly(seModel, keyWords);

	if(fileOut)
	{
		for(j=0;j<3;j++)
			for( i=0; i<m.nAtoms;i++)
				m.gradient[j][i] = 0.0;
		if(!getGradientFireFly(fileOut, seModel))
		{
#ifdef G_OS_WIN32
			gchar* comm = g_strdup_printf("type %s",fileOut);
#else
			gchar* comm = g_strdup_printf("cat %s",fileOut);
#endif
			StopCalcul=TRUE;
			set_text_to_draw(_("Problem : I cannot caculate the Gradient... "));
			set_statubar_operation_str(_("Calculation Stopped "));
			drawGeom();
			gtk_widget_set_sensitive(StopButton, FALSE);
			Waiting(1);
			{int ierr = system(comm);}
			g_free(fileOut);
			g_free(comm);
			return;
		}
		getEnergyFireFly(fileOut, &seModel->molecule.energy);
		g_free(fileOut);
	}

}
/**********************************************************************/
static void calculateEnergyFireFly(SemiEmpiricalModel* seModel)
{
	gchar* keyWords = NULL;
	gchar* fileOut = NULL;
	if(!seModel) return;
	if(seModel->molecule.nAtoms<1) return;
	if(!seModel->method) return;
	keyWords = g_strdup_printf("RUNTYP=Energy GBASIS=%s",seModel->method);
	fileOut = runOneFireFly(seModel, keyWords);
	if(fileOut)
	{
		getEnergyFireFly(fileOut, &seModel->molecule.energy);
		g_free(fileOut);
	}

}
/**********************************************************************/
SemiEmpiricalModel createFireFlyModel (GeomDef* geom,gint Natoms,gint charge, gint spin, gchar* method, gchar* dirName, SemiEmpiricalModelConstraints constraints)
{
	SemiEmpiricalModel seModel = newFireFlyModel(method,dirName, constraints);

	seModel.molecule = createMoleculeSE(geom,Natoms, charge, spin,TRUE);
	setRattleConstraintsParameters(&seModel);
	
	return seModel;
}
/**********************************************************************/
