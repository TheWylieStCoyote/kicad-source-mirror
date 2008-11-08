/********************************************************/
/* Routine de lecture d'un fichier GERBER format RS274X */
/********************************************************/

#include "fctsys.h"

#include "common.h"
#include "gerbview.h"
#include "pcbplot.h"

#include "protos.h"

#define CODE( x, y ) ( ((x) << 8) + (y) )

enum RS274X_PARAMETERS
{
    FORMAT_STATEMENT    = CODE( 'F', 'S' ),
    AXIS_SELECT         = CODE( 'A', 'S' ),
    MIRROR_IMAGE        = CODE( 'M', 'I' ),
    MODE_OF_UNITS       = CODE( 'M', 'O' ),
    INCH                = CODE( 'I', 'N' ),
    MILLIMETER          = CODE( 'M', 'M' ),
    OFFSET              = CODE( 'O', 'F' ),
    SCALE_FACTOR        = CODE( 'S', 'F' ),

    IMAGE_NAME          = CODE( 'I', 'N' ),
    IMAGE_JUSTIFY       = CODE( 'I', 'J' ),
    IMAGE_OFFSET        = CODE( 'I', 'O' ),
    IMAGE_POLARITY      = CODE( 'I', 'P' ),
    IMAGE_ROTATION      = CODE( 'I', 'R' ),
    PLOTTER_FILM        = CODE( 'P', 'M' ),
    INCLUDE_FILE        = CODE( 'I', 'F' ),

    AP_DEFINITION       = CODE( 'A', 'D' ),

    AP_MACRO            = CODE( 'A', 'M' ),
    LAYER_NAME          = CODE( 'L', 'N' ),
    LAYER_POLARITY      = CODE( 'L', 'P' ),
    KNOCKOUT            = CODE( 'K', 'O' ),
    STEP_AND_REPEAT     = CODE( 'S', 'P' ),
    ROTATE              = CODE( 'R', 'O' )
};


/* Local Functions */


/**
 * Function ReadXCommand
 * reads int two bytes of data and assembles them into an int with the first
 * byte in the sequence put into the most significant part of a 16 bit value
 * and the second byte put into the least significant part of the 16 bit value.
 * @param text A reference to a pointer to read bytes from and to advance as they are read.
 * @return int - with 16 bits of data in the ls bits, upper bits zeroed.
 */
static int ReadXCommand( char*& text )
{
    int result;

    if( text && *text )
        result = *text++ << 8;
    else
        return -1;

    if( text && *text )
        result += *text++;
    else
        return -1;

    return result;
}


/**
 * Function ReadInt
 * reads an int from an ASCII character buffer.  If there is a comma after the
 * int, then skip over that.
 * @param text A reference to a character pointer from which bytes are read
 *    and the pointer is advanced for each byte read.
 * @param int - The int read in.
 */
static int ReadInt( char*& text )
{
    int ret = (int) strtol( text, &text, 10 );

    if( *text == ',' )
        ++text;

    return ret;
}


/**
 * Function ReadDouble
 * reads a double in from a character buffer. If there is a comma after the double,
 * then skip over that.
 * @param text A reference to a character pointer from which the ASCII double
 *          is read from and the pointer advanced for each character read.
 * @return double
 */
static double ReadDouble( char*& text )
{
    double ret = strtod( text, &text );

    if( *text == ',' )
        ++text;

    return ret;
}


/****************************************************************************/
bool GERBER::ReadRS274XCommand( WinEDA_GerberFrame* frame, wxDC* DC,
                                      char buff[GERBER_BUFZ], char*& text )
/****************************************************************************/
{
    bool ok = true;
    int  code_command;

    text++;

    for(;;)
    {
        while( *text )
        {
            switch( *text )
            {
            case '%':       // end of command
                text++;
                m_CommandState = CMD_IDLE;
                goto exit;  // success completion

            case ' ':
            case '\r':
            case '\n':
                text++;
                break;

            case '*':
                text++;
                break;

            default:
                code_command = ReadXCommand( text );
                ok = ExecuteRS274XCommand( code_command, buff, text );
                if( !ok )
                    goto exit;
                break;
            }
        }

        // end of current line, read another one.
        if( fgets( buff, GERBER_BUFZ, m_Current_File ) == NULL )
        {
            // end of file
            ok = false;
            break;
        }

        text = buff;
    }

exit:
    return ok;
}


/*******************************************************************************/
bool GERBER::ExecuteRS274XCommand( int command, char buff[GERBER_BUFZ], char*& text )
/*******************************************************************************/
{
    int      code;
    int      xy_seq_len, xy_seq_char;
    char     ctmp;
    bool     ok = TRUE;
    D_CODE*  dcode;
    char     line[GERBER_BUFZ];
    wxString msg;
    double   fcoord;
    double   conv_scale = m_GerbMetric ? PCB_INTERNAL_UNIT / 25.4 : PCB_INTERNAL_UNIT;

    switch( command )
    {
    case FORMAT_STATEMENT:
        xy_seq_len = 2;

        while( *text != '*' )
        {
            switch( *text )
            {
            case ' ':
                text++;
                break;

            case 'L':       // No Leading 0
                m_NoTrailingZeros = FALSE;
                text++;
                break;

            case 'T':       // No trailing 0
                m_NoTrailingZeros = TRUE;
                text++;
                break;

            case 'A':           // Absolute coord
                m_Relative = FALSE;
                text++;
                break;

            case 'I':           // Absolute coord
                m_Relative = TRUE;
                text++;
                break;

            case 'N':           // Sequence code (followed by the number of digits for the X,Y command
                text++;
                xy_seq_char = *text++;
                if( (xy_seq_char >= '0') && (xy_seq_char <= '9') )
                    xy_seq_len = -'0';
                break;

            case 'X':
            case 'Y':           // Valeurs transmises :2 (really xy_seq_len : FIX ME) digits
                code = *(text++);
                ctmp = *(text++) - '0';
                if( code == 'X' )
                {
                    m_FmtScale.x = *text - '0';                 // = nb chiffres apres la virgule
                    m_FmtLen.x   = ctmp + m_FmtScale.x;         // = nb total de chiffres
                }
                else
                {
                    m_FmtScale.y = *text - '0';
                    m_FmtLen.y   = ctmp + m_FmtScale.y;
                }
                text++;
                break;

            case '*':
                break;

            default:
                GetEndOfBlock( buff, text, m_Current_File );
                ok = FALSE;
                break;
            }
        }
        break;

    case AXIS_SELECT:
    case MIRROR_IMAGE:
        ok = FALSE;
        break;

    case MODE_OF_UNITS:
        code = ReadXCommand( text );
        if( code == INCH )
            m_GerbMetric = FALSE;
        else if( code == MILLIMETER )
            m_GerbMetric = TRUE;
        conv_scale = m_GerbMetric ? PCB_INTERNAL_UNIT / 25.4 : PCB_INTERNAL_UNIT;
        break;

    case OFFSET:        // command: OFAnnBnn (nn = float number)
        m_Offset.x = m_Offset.y = 0;
        while( *text != '*' )
        {
            switch( *text )
            {
            case 'A':       // A axis offset in current unit (inch ou mm)
                text++;
                fcoord     = ReadDouble( text );
                m_Offset.x = (int) round( fcoord * conv_scale );
                break;

            case 'B':       // B axis offset in current unit (inch ou mm)
                text++;
                fcoord     = ReadDouble( text );
                m_Offset.y = (int) round( fcoord * conv_scale );
                break;
            }
        }

        break;

    case SCALE_FACTOR:
    case IMAGE_JUSTIFY:
    case IMAGE_ROTATION:
    case IMAGE_OFFSET:
    case PLOTTER_FILM:
    case LAYER_NAME:
    case KNOCKOUT:
    case STEP_AND_REPEAT:
    case ROTATE:
        msg.Printf( _( "Command <%c%c> ignored by Gerbview" ),
                    (command >> 8) & 0xFF, command & 0xFF );
        if( g_DebugLevel > 0 )
            wxMessageBox( msg );
        break;

    case IMAGE_NAME:
        m_Name.Empty();
        while( *text != '*' )
        {
            m_Name.Append( *text++ );
        }

        break;

    case IMAGE_POLARITY:
        if( strnicmp( text, "NEG", 3 ) == 0 )
            m_ImageNegative = TRUE;
        else
            m_ImageNegative = FALSE;
        break;

    case LAYER_POLARITY:
        if( *text == 'C' )
            m_LayerNegative = TRUE;
        else
            m_LayerNegative = FALSE;
        break;

    case AP_MACRO:
        ReadApertureMacro( buff, text, m_Current_File );
        break;

    case INCLUDE_FILE:
        if( m_FilesPtr >= 10 )
        {
            ok = FALSE;
            DisplayError( NULL, _( "Too many include files!!" ) );
            break;
        }
        strcpy( line, text );
        strtok( line, "*%%\n\r" );
        m_FilesList[m_FilesPtr] = m_Current_File;

        m_Current_File = fopen( line, "rt" );
        if( m_Current_File == 0 )
        {
            wxString msg;
            msg.Printf( wxT( "fichier <%s> non trouve" ), line );
            DisplayError( NULL, msg, 10 );
            ok = FALSE;
            m_Current_File = m_FilesList[m_FilesPtr];
            break;
        }
        m_FilesPtr++;
        break;

    case AP_DEFINITION:
        if( *text != 'D' )
        {
            ok = FALSE;
            break;
        }
        m_As_DCode = TRUE;
        text++;

        code  = ReadInt( text );
        ctmp  = *text;

        dcode = ReturnToolDescr( m_Layer, code );
        if( dcode == NULL )
            break;

        if( text[1] == ',' )        // Tool usuel (C,R,O,P)
        {
            text += 2;              // text pointe size ( 1er modifier)
            dcode->m_Size.x = dcode->m_Size.y =
                                  (int) round( ReadDouble( text ) * conv_scale );

            switch( ctmp )
            {
            case 'C':               // Circle
                dcode->m_Shape = APT_CIRCLE;
                while( *text == ' ' )
                    text++;

                if( *text == 'X' )
                {
                    text++;
                    dcode->m_Drill.x = dcode->m_Drill.y =
                                           (int) round( ReadDouble( text ) * conv_scale );
                    dcode->m_DrillShape = 1;
                }

                while( *text == ' ' )
                    text++;

                if( *text == 'X' )
                {
                    text++;
                    dcode->m_Drill.y =
                        (int) round( ReadDouble( text ) * conv_scale );

                    dcode->m_DrillShape = 2;
                }
                dcode->m_Defined = TRUE;
                break;

            case 'O':               // oval
            case 'R':               // rect
                dcode->m_Shape = (ctmp == 'O') ? APT_OVAL : APT_RECT;

                while( *text == ' ' )
                    text++;

                if( *text == 'X' )
                {
                    text++;
                    dcode->m_Size.y =
                        (int) round( ReadDouble( text ) * conv_scale );
                }

                while( *text == ' ' )
                    text++;

                if( *text == 'X' )
                {
                    text++;
                    dcode->m_Drill.x = dcode->m_Drill.y =
                                           (int) round( ReadDouble( text ) * conv_scale );
                    dcode->m_DrillShape = 1;
                }

                while( *text == ' ' )
                    text++;

                if( *text == 'Y' )
                {
                    text++;
                    dcode->m_Drill.y =
                        (int) round( ReadDouble( text ) * conv_scale );
                    dcode->m_DrillShape = 2;
                }
                dcode->m_Defined = TRUE;
                break;

            case 'P':               // Polygon
                dcode->m_Shape   = APT_POLYGON;
                dcode->m_Defined = TRUE;
                break;
            }
        }
        break;

    default:
        ok = FALSE;
        break;
    }

    ok = GetEndOfBlock( buff, text, m_Current_File );

    return ok;
}


/*****************************************************************/
bool GetEndOfBlock( char buff[GERBER_BUFZ], char*& text, FILE* gerber_file )
/*****************************************************************/
{
    for(;;)
    {
        while( (text < buff + GERBER_BUFZ) && *text )
        {
            if( *text == '*' )
                return TRUE;

            if( *text == '%' )
                return TRUE;

            text++;
        }

        if( fgets( buff, GERBER_BUFZ, gerber_file ) == NULL )
            break;

        text = buff;
    }

    return FALSE;
}


/*******************************************************************/
bool GERBER::ReadApertureMacro( char buff[GERBER_BUFZ], char*& text, FILE* gerber_file )
/*******************************************************************/
{
    APERTURE_MACRO  am;

    // read macro name
    while( *text )
    {
        if( *text == '*' )
        {
            ++text;
            break;
        }

        am.name.Append( *text++ );
    }

    if( g_DebugLevel > 0 )
        wxMessageBox( am.name, wxT( "macro name" ) );

    for(;;)
    {
        AM_PRIMITIVE    prim;

        if( *text == '*' )
            ++text;

        if( *text == '\n' )
            ++text;

        if( !*text )
        {
            text = buff;
            if( fgets( buff, GERBER_BUFZ, gerber_file ) == NULL )
                return false;
        }

        if( *text == '%' )
            break;      // exit with text still pointing at %

        prim.primitive_id = (AM_PRIMITIVE_ID) ReadInt( text );

        int paramCount;

        switch( prim.primitive_id )
        {
        default:
        case AMP_CIRCLE:
            paramCount = 4;
            break;
        case AMP_LINE2:
        case AMP_LINE20:
            paramCount = 7;
            break;
        case AMP_LINE_CENTER:
        case AMP_LINE_LOWER_LEFT:
            paramCount = 6;
            break;
        case AMP_EOF:
            paramCount = 0;
            break;
        case AMP_OUTLINE:
            paramCount = 4;
            break;
        case AMP_POLYGON:
            paramCount = 4;
            break;
        case AMP_MOIRE:
            paramCount = 9;
            break;
        case AMP_THERMAL:
            paramCount = 6;
            break;
        }

        DCODE_PARAM param;

        for( int i=0;  i<paramCount && *text && *text!='*';  ++i )
        {
            if( *text == '$' )
            {
                ++text;
                param.isImmediate = false;
            }
            else
            {
                param.isImmediate = true;
            }

            param.value = ReadDouble( text );
            prim.params.push_back( param );
        }

        if( prim.primitive_id == AMP_OUTLINE )
        {
            paramCount = (int) prim.params[1].value * 2 + 1;

            for( int i=0; i<paramCount && *text && *text!='*';  ++i )
            {
                if( *text == '$' )
                {
                    ++text;
                    param.isImmediate = false;
                }
                else
                {
                    param.isImmediate = true;
                }

                param.value = ReadDouble( text );
                prim.params.push_back( param );
            }
        }

        am.primitives.push_back( prim );
    }

    m_aperture_macros.insert( am );

    return true;
}

